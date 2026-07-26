#include "integrator.h"
#include "config.h"
#include <thread>

Integrator::Integrator(Scene* _scene, Film* _film): scene(_scene), film(_film) {}

void Integrator::render() {
  film->clear();

  int numThreads = (int)std::thread::hardware_concurrency();
  std::vector<MTRandom> samplers(numThreads);
  std::atomic<unsigned int> tileId(0);

  std::vector<std::thread> threads;
  threads.reserve(numThreads);

  for (int i = 0; i < numThreads; i++) {
    threads.emplace_back(&Integrator::renderTile, this, i, std::ref(tileId), std::ref(samplers[i]));
  }

  for (auto& t : threads) t.join();

  film->incrementSPP();
}

void Integrator::renderTile(int threadId, std::atomic<unsigned int>& tileId, MTRandom& sampler) {
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
        float px = x + sampler.next();
        float py = y + sampler.next();

        Ray ray = scene->camera.generateRay(px, py);
        Colour col = integrate(ray, &sampler);
        film->splat(px, py, col);
      }
    }
  }
}

// Colour Integrator::computeDirectMIS(const ShadingData& sd, Sampler* sampler) {

// }

void Integrator::lightSamplingMIS(ShadingData shadingData, Sampler* sampler, Colour &result) {
  float pmf;
	Light* sampledLight = scene->sampleLightWeighted(sampler, pmf);
  if (!sampledLight) return;

  LightSamplingMISResult misRes; 

  if (sampledLight->isArea()) {
    misRes = lightSamplingMISAreaLight(shadingData, sampler, sampledLight, pmf);
  } else {
    misRes = lightSamplingMISEnvMap(shadingData, sampler, sampledLight, pmf);
  }

  float brdfPDF = shadingData.bsdf->PDF(shadingData, misRes.wi);
  brdfPDF = brdfPDF * misRes.cosThetaLine / misRes.distSqr;

  float weight = misRes.pdf / (misRes.pdf + brdfPDF);

  if (misRes.pdf > 0) {
    result = result + (misRes.finalColor / misRes.pdf) * weight;
  }
}

LightSamplingMISResult Integrator::lightSamplingMISAreaLight(ShadingData shadingData, Sampler* sampler, Light* sampledLight, float pmf) {
  Colour emittedColour;
  float pdf;
  Vec3 sampledPoint = sampledLight->sample(shadingData, sampler, emittedColour, pdf);

  Vec3 wi = sampledPoint - shadingData.x;
  wi = wi.normalize();

  float cosTheta = shadingData.sNormal.dot(wi);
  if (cosTheta < 0) cosTheta = 0;

  Vec3 normalLine = sampledLight->normal(shadingData, wi);
  float cosThetaLine = -wi.dot(normalLine);
  if (cosThetaLine < 0) cosThetaLine = 0;

  float distSqr = (shadingData.x - sampledPoint).lengthSq();

  float gTerm = (cosTheta * cosThetaLine) / distSqr;
  bool isVisible = scene->visible(shadingData.x, sampledPoint);

  float fullPdf = pmf * pdf;
  Colour finalColor = shadingData.bsdf->evaluate(shadingData, wi) * emittedColour * gTerm * isVisible;
  return { finalColor, wi, fullPdf, cosThetaLine, distSqr, gTerm };
}

LightSamplingMISResult Integrator::lightSamplingMISEnvMap(ShadingData shadingData, Sampler* sampler, Light* sampledLight, float pmf) {
  Colour emittedColour;
  float pdf;
  Vec3 wi = sampledLight->sample(shadingData, sampler, emittedColour, pdf);

  float cosTheta = shadingData.sNormal.dot(wi);
  if (cosTheta < 0) cosTheta = 0;

  float gTerm = cosTheta;

  float maxDist = (scene->bounds.bmax - scene->bounds.bmin).length();
  Vec3 farPoint = shadingData.x + (wi * maxDist);

  bool isVisible = scene->visible(shadingData.x, farPoint);

  float resultGTerm = gTerm * isVisible;
  float fullPdf = pmf * pdf;
  Colour finalColor = shadingData.bsdf->evaluate(shadingData, wi) * emittedColour * resultGTerm;

  return { finalColor, wi, fullPdf, 1.0f, 1.0f, gTerm };
}
