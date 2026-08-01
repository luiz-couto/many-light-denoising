#include "instant_radiosity.h"
#include "core.h"
#include "light.h"
#include "config.h"

void InstantRadiosityIntegrator::prepare(Sampler* sampler) {
  vpls.clear();
  vpls.reserve(Config::IR_NUM_LIGHT_PATHS * Config::IR_MAX_PHOTON_DEPTH);

  for (int i=0; i<Config::IR_NUM_LIGHT_PATHS; i++) {
    Ray ray;

    Colour flux = emitPhoton(sampler, ray) / (float)Config::IR_NUM_LIGHT_PATHS;
    if (flux.lum() <= 0.0f) continue;

    for (int depth = 0; depth < Config::IR_MAX_PHOTON_DEPTH; depth++) {
      IntersectionData intersection = scene->traverse(ray);
      if (intersection.t == FLT_MAX) break;

      ShadingData shadingData = scene->calculateShadingData(intersection, ray);
      Colour albedoAtHit = shadingData.bsdf->diffuseAlbedo(shadingData);
      if (albedoAtHit.lum() > 0.0f) {
        depositVPL(shadingData, flux, albedoAtHit);
      }

      Colour weight;
      float pdf;
      Vec3 newDirection = shadingData.bsdf->sample(shadingData, sampler, weight, pdf);
      if (pdf <= 0.0f || weight.lum() <= 0.0f) break;
      
      flux = flux * weight;

      // RR
      if (depth >= 1) {
        float q = std::min(weight.lum(), 1.0f);
        if (sampler->next() > q) break;
        flux = flux / q;
      }

      ray.init(shadingData.x + (newDirection * RAY_EPSILON), newDirection);
    }
  }
}

Colour InstantRadiosityIntegrator::emitPhoton(Sampler* sampler, Ray& emittedRay) {
  float pmf;
	Light* sampledLight = scene->sampleLightWeighted(sampler, pmf);

  float posLightPdf;
  Vec3 positionFromLight = sampledLight->samplePositionFromLight(sampler, posLightPdf);

  float dirLightPdf;
  Vec3 directionFromLight = sampledLight->sampleDirectionFromLight(sampler, dirLightPdf);

  if (posLightPdf <= 0.0f || dirLightPdf <= 0.0f) return Colour(0.0f, 0.0f, 0.0f);

  if (sampledLight->isArea()) {
    Colour photonFlux = sampledLight->evaluate(-directionFromLight) * PI / (pmf * posLightPdf);
    emittedRay.init(positionFromLight + (directionFromLight * RAY_EPSILON), directionFromLight);
    return photonFlux;
  }

  Vec3 sceneCentre = (scene->bounds.bmax + scene->bounds.bmin) * 0.5f;
  Vec3 inwardNormal = (sceneCentre - positionFromLight).normalize();
  float cosEmit = std::max(0.0f, -directionFromLight.dot(inwardNormal));
  
  Colour photonFlux = sampledLight->evaluate(directionFromLight) * cosEmit / (pmf * posLightPdf * dirLightPdf);
  emittedRay.init(positionFromLight + (-directionFromLight * RAY_EPSILON), -directionFromLight);
  
  return photonFlux;
}

void InstantRadiosityIntegrator::depositVPL(const ShadingData& shadingData, const Colour& flux, const Colour& albedoAtHit) {
  Colour radiance = flux * albedoAtHit / PI;

  VPL vpl;
  vpl.position = shadingData.x;
  vpl.normal   = shadingData.sNormal;
  vpl.footprintRadius = 0.0f;
  vpl.radiance = radiance;
  vpls.push_back(vpl);
}

Colour InstantRadiosityIntegrator::gatherVPLs(const ShadingData& shadingData) {
  Colour indirectLight = Colour(0.0f, 0.0f, 0.0f);

  for (const VPL& vpl : vpls) {
    Vec3 toVPL = vpl.position - shadingData.x;
    float distSqr = toVPL.lengthSq();
    if (distSqr <= 1e-12f) continue; // VPL at x itself, avoid NaN from normalize

    Vec3 wi = toVPL.normalize();
    float cosX = std::max(0.0f, shadingData.sNormal.dot(wi));
    float cosVPL = std::max(0.0f, vpl.normal.dot(-wi));
    if (cosX <= 0.0f || cosVPL <= 0.0f) continue;  // geometrically dark, no ray needed

    if (!scene->visible(shadingData.x, vpl.position)) continue;

    float distance = 1.0f / distSqr;
    float g = std::min((cosX * cosVPL) * distance, Config::IR_G_CLAMP);
    Colour bsdf = shadingData.bsdf->evaluate(shadingData, wi);

    Colour total = bsdf * g * vpl.radiance;
    indirectLight = indirectLight + total;
  }

  return indirectLight;
}

Colour InstantRadiosityIntegrator::integrate(const Ray& ray, Sampler* sampler) {
  Ray currentRay = ray;
  Colour throughput(1.0f, 1.0f, 1.0f);

  for (int depth=0; depth<=Config::IR_MAX_SPECULAR_DEPTH; depth++) {
    IntersectionData intersection = scene->traverse(currentRay);
    ShadingData shadingData = scene->calculateShadingData(intersection, currentRay);

    // miss -> environment
    if (shadingData.t == FLT_MAX) {
      return throughput * scene->background->evaluate(currentRay.dir);
    }

    // looking straight at a lamp (directly or via mirrors/glass)
    if (shadingData.bsdf->isLight()) {
      return throughput * shadingData.bsdf->emit(shadingData, shadingData.wo);
    }

    // pure specular (mirror/glass): follow the reflection/refraction
    if (shadingData.bsdf->isPureSpecular()) {
      Colour weight;
      float pdf;
      Vec3 newDirection = shadingData.bsdf->sample(shadingData, sampler, weight, pdf);
      if (pdf <= 0.0f || weight.lum() <= 0.0f) return Colour(0.0f, 0.0f, 0.0f);

      throughput = throughput * weight;
      currentRay.init(shadingData.x + (newDirection * RAY_EPSILON), newDirection);
      continue;
    }

    // the gather point: first non-specular surface on the chain
    return throughput * (computeDirectMIS(shadingData, sampler) + gatherVPLs(shadingData));
  }

  return Colour(0.0f, 0.0f, 0.0f);   // specular chain exceeded IR_MAX_SPECULAR_DEPTH
}
