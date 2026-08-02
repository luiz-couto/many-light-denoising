#include "restir_instant_radiosity.h"
#include "config.h"
#include "reservoir.h"
#include <thread>

static unsigned int seedFor(int pixelIndex, int spp, unsigned int stream) {
  return (unsigned int)pixelIndex * 9781u + (unsigned int)spp * 6271u + stream * 26699u + 1u;
}

void ReSTIRInstantRadiosityIntegrator::render() {
  size_t pixelCount = (size_t)scene->width * scene->height;
  if (gBuffer.size() != pixelCount) {
    gBuffer.resize(pixelCount);
    reservoirs.resize(pixelCount);
    reservoirsPrev.resize(pixelCount);
  }

  MTRandom prepareSampler(film->SPP + 1);
  prepare(&prepareSampler);

  runTiled([this](int x, int y) {
    int pixelIndex = y * scene->width + x;
    MTRandom selection(seedFor(pixelIndex, film->SPP, 0));
    tracePrimary(x, y, &selection);
    generateCandidates(pixelIndex, &selection);
  });

  for (int round = 0; round < Config::IR_RESTIR_SPATIAL_ROUNDS; round++) {
    std::swap(reservoirs, reservoirsPrev);
    runTiled([this, round](int x, int y) {
      MTRandom reuse(seedFor(y * scene->width + x, film->SPP, 2 + round));
      spatialReuse(x, y, round, &reuse);
    });
  }

  runTiled([this](int x, int y) {
    int pixelIndex = y * scene->width + x;
    MTRandom shading(seedFor(pixelIndex, film->SPP, 1));
    film->splat((float)x, (float)y, shadePixel(pixelIndex, &shading));
  });

  film->incrementSPP();
}

void ReSTIRInstantRadiosityIntegrator::runTiled(const std::function<void(int x, int y)>& pixelFunc) {
  int numThreads = (int)std::thread::hardware_concurrency();
  std::atomic<unsigned int> tileId(0);

  std::vector<std::thread> threads;
  threads.reserve(numThreads);

  for (int i = 0; i < numThreads; i++) {
    threads.emplace_back(
      &ReSTIRInstantRadiosityIntegrator::runPixelFunc,
      this,
      std::ref(tileId),
      pixelFunc
    );
  }

  for (auto& t : threads) t.join();
}

void ReSTIRInstantRadiosityIntegrator::runPixelFunc(std::atomic<unsigned int>& tileId, const std::function<void(int x, int y)>& pixelFunc) {
  int tilesX = (scene->width  + Config::TILE_SIZE - 1) / Config::TILE_SIZE;
  int tilesY = (scene->height + Config::TILE_SIZE - 1) / Config::TILE_SIZE;
  int totalTiles = tilesX * tilesY;

  while (true) {
    unsigned int tile = tileId++;
    if ((int)tile >= totalTiles) break;

    int tileCol = tile % tilesX;
    int tileRow = tile / tilesX;
    int xStart = tileCol * Config::TILE_SIZE;
    int yStart = tileRow * Config::TILE_SIZE;
    int xEnd = std::min(xStart + Config::TILE_SIZE, scene->width);
    int yEnd = std::min(yStart + Config::TILE_SIZE, scene->height);

    for (int y = yStart; y < yEnd; y++) {
      for (int x = xStart; x < xEnd; x++) {
        pixelFunc(x, y);
      }
    }
  }
}

void ReSTIRInstantRadiosityIntegrator::tracePrimary(int x, int y, Sampler* sampler) {
  float px = (float)x + sampler->next();
  float py = (float)y + sampler->next();
  Ray currentRay = scene->camera.generateRay(px, py);

  PrimaryHit& hit = gBuffer[y * scene->width + x];
  hit = PrimaryHit{};
  Colour throughput(1.0f, 1.0f, 1.0f);

  for (int depth = 0; depth <= Config::IR_MAX_SPECULAR_DEPTH; depth++) {
    IntersectionData intersection = scene->traverse(currentRay);
    ShadingData shadingData = scene->calculateShadingData(intersection, currentRay);

    if (shadingData.t == FLT_MAX) {
      hit.resolved = throughput * scene->background->evaluate(currentRay.dir);
      return;
    }

    if (shadingData.bsdf->isLight()) {
      hit.resolved = throughput * shadingData.bsdf->emit(shadingData, shadingData.wo);
      return;
    }

    if (shadingData.bsdf->isPureSpecular()) {
      Colour weight;
      float pdf;
      Vec3 newDirection = shadingData.bsdf->sample(shadingData, sampler, weight, pdf);
      if (pdf <= 0.0f || weight.lum() <= 0.0f) return;
      throughput = throughput * weight;
      currentRay.init(shadingData.x + (newDirection * RAY_EPSILON), newDirection);
      continue;
    }

    if (film->SPP == 0) {
      film->setNormal(x, y, Colour(shadingData.sNormal.x, shadingData.sNormal.y, shadingData.sNormal.z));
      film->setAlbedo(x, y, shadingData.bsdf->evaluate(shadingData, shadingData.wo));
    }

    hit.shadingData = shadingData;
    hit.throughput = throughput;
    hit.needsGather = true;
    return;
  }
}

float ReSTIRInstantRadiosityIntegrator::pHat(const ShadingData& shadingData, const VPL& vpl) {
  return unshadowedVPLContribution(shadingData, vpl).lum();
}

void ReSTIRInstantRadiosityIntegrator::generateCandidates(int pixelIndex, Sampler* sampler) {
  Reservoir reservoir;
  
  const PrimaryHit& hit = gBuffer[pixelIndex];
  if (!hit.needsGather || vpls.empty()) {
    reservoirs[pixelIndex] = reservoir; // clean empty reservoir
    return;
  }

  float poolSize = (float)vpls.size();

  for (int m=0; m<Config::IR_RESTIR_M; m++) {
    // uniform draw, clamped for the next() == 1.0-epsilon edge
    int candidateIndex = std::min((int)(sampler->next() * poolSize), (int)vpls.size() - 1);
    
    // w = pHat / q  with q = 1/N  ->  w = pHat * N
    float weight = pHat(hit.shadingData, vpls[candidateIndex]) * poolSize;

    reservoir.update(candidateIndex, weight, sampler->next());
  }

  if (reservoir.vplIndex >= 0) {
    reservoir.finalize(pHat(hit.shadingData, vpls[reservoir.vplIndex]));
  }

  reservoirs[pixelIndex] = reservoir;
}


void ReSTIRInstantRadiosityIntegrator::spatialReuse(int x, int y, int round, Sampler* sampler) {
  int pixelIndex = y * scene->width + x;
  Reservoir merged = reservoirsPrev[pixelIndex];

  const PrimaryHit& hit = gBuffer[pixelIndex];
  if (!hit.needsGather) {
    reservoirs[pixelIndex] = merged;  // copy-through keeps the write buffer defined
    return;
  }

  for (int k=0; k<Config::IR_RESTIR_K; k++) {
    int neighbourIndex = selectRandomNeighbour(x, y, sampler);
    if (neighbourIndex < 0) continue;

    const PrimaryHit& neighbourHit = gBuffer[neighbourIndex];
    if (!neighbourHit.needsGather) continue;

    const Reservoir& neighbourReservoir = reservoirsPrev[neighbourIndex];
    if (neighbourReservoir.vplIndex < 0) continue; // empty: nothing to learn

    // check if neighbour is in a different "surface"
    if (hit.shadingData.sNormal.dot(neighbourHit.shadingData.sNormal) < Config::IR_RESTIR_NORMAL_THRESHOLD) continue;
    if (std::abs(hit.shadingData.t - neighbourHit.shadingData.t) > Config::IR_RESTIR_DEPTH_THRESHOLD * hit.shadingData.t) continue;

    float pHatAtMine = pHat(hit.shadingData, vpls[neighbourReservoir.vplIndex]);
    merged.merge(neighbourReservoir, pHatAtMine, sampler->next());
  }

  // The merge may have changed the winner: the old W is stale. Re-finalize.
  if (merged.vplIndex >= 0) {
    merged.finalize(pHat(hit.shadingData, vpls[merged.vplIndex]));
  }

  reservoirs[pixelIndex] = merged;
}

int ReSTIRInstantRadiosityIntegrator::selectRandomNeighbour(int x, int y, Sampler* sampler) {
  float angle  = 2.0f * PI * sampler->next();
  float radius = Config::IR_RESTIR_RADIUS * sqrtf(sampler->next());
  int neighbourX = x + (int)std::round(radius * cosf(angle));
  int neighbourY = y + (int)std::round(radius * sinf(angle));

  if (neighbourX < 0 || neighbourX >= scene->width)  return -1;
  if (neighbourY < 0 || neighbourY >= scene->height) return -1;
  if (neighbourX == x && neighbourY == y) return -1;

  return neighbourY * scene->width + neighbourX;
}

Colour ReSTIRInstantRadiosityIntegrator::shadePixel(int pixelIndex, Sampler* sampler) {
  const PrimaryHit& hit = gBuffer[pixelIndex];
  if (!hit.needsGather) return hit.resolved; // sky/lamp/mirror-overflow

  Colour direct = computeDirectMIS(hit.shadingData, sampler);
  Colour indirect(0.0f, 0.0f, 0.0f);

  const Reservoir& reservoir = reservoirs[pixelIndex];
  if (reservoir.vplIndex >= 0 && reservoir.contributionWeight > 0.0f) {
    const VPL& winner = vpls[reservoir.vplIndex];
    Colour contribution = unshadowedVPLContribution(hit.shadingData, winner);

    // Only place we actually cast a ray
    if (contribution.lum() > 0.0f && scene->visible(hit.shadingData.x, winner.position)) {
      indirect = contribution * reservoir.contributionWeight;
    }
  }

  return hit.throughput * (direct + indirect);
}
