#include "path_tracer.h"
#include "core.h"
#include "geometry.h"
#include "config.h"

Colour PathTracerIntegrator::integrate(const Ray& ray, Sampler* sampler) {
  Colour start = Colour(1.0f, 1.0f, 1.0f);
  return pathTrace(ray, start, 0, 0.0f, sampler, false);
}

Colour PathTracerIntegrator::pathTrace(const Ray& ray, Colour throughput, int depth, float bsdfPDF, Sampler* sampler, bool isSpecularBounce) {
  if (depth > Config::PT_MAX_DEPTH) {
		return Colour(0.0f, 0.0f, 0.0f);
	}

  IntersectionData intersection = scene->traverse(ray);
	ShadingData shadingData = scene->calculateShadingData(intersection, ray);

  // miss
  if (shadingData.t == FLT_MAX) {
    Colour envRadiance = throughput * scene->background->evaluate(ray.dir);
    if (isSpecularBounce || depth == 0) {
      return envRadiance;
    }

    //  BSDF sampling branch of MIS
    float envPDF = scene->environmentLightSelectionPDF(shadingData, ray.dir);
    float denom  = bsdfPDF + envPDF;
    float weight = (denom > 0.0f) ? bsdfPDF / denom : 1.0f;
    return envRadiance * weight;
  }

  // hit emissive
  if (shadingData.bsdf->isLight()) {
    Colour emission = shadingData.bsdf->emit(shadingData, shadingData.wo);
    if (isSpecularBounce || depth == 0) {
      return throughput * emission;
    }

    //  BSDF sampling branch of MIS
    float distSq = intersection.t * intersection.t;
    float cosThetaLight = fabsf(shadingData.sNormal.dot(shadingData.wo));
    if (cosThetaLight <= 0.0f || distSq <= 0.0f) {
      return throughput * emission;
    }

    float lightPDF = scene->areaLightSelectionPDF(intersection.ID) * distSq / cosThetaLight;
    float denom  = bsdfPDF + lightPDF;
    float weight = (denom > 0.f) ? bsdfPDF / denom : 1.0f;
    return throughput * emission * weight;
  }

  Colour indirect;
	float pdf;
  Vec3 worldDirection = shadingData.bsdf->sample(shadingData, sampler, indirect, pdf);

  Colour directLight = computeDirectMIS(shadingData, sampler);
  float cosTheta = std::max(fabsf(worldDirection.dot(shadingData.sNormal)), 0.0f);

  if (cosTheta < 1e-6f || pdf <= 0.0f) {
		return directLight * throughput;
	}

  Colour newThroughput = throughput * indirect;

  // RR on the MAX channel: q >= every channel of throughput's step, so no
  // channel can grow through a surviving bounce. Luminance-based q divided
  // blue-dominant throughput by its tiny luminance each survival (blue's
  // luminance weight is 0.07), compounding to float overflow and NaN pixels
  // under blue-heavy env light (classroom: 2839 NaN px in one spp-256 run;
  // same fix as the photon pass, 2026-08-30).
  if (depth >= Config::PT_RR_DEPTH) {
    float q = std::max(newThroughput.r, std::max(newThroughput.g, newThroughput.b));
    float qClamped = std::min(q, 1.0f);
    float epsilon = sampler->next();
    if (epsilon > qClamped) {
      return directLight * throughput;
    }

    newThroughput = newThroughput / qClamped;
  }

  // Recursive
  Ray newRay;
  newRay.init(shadingData.x + (worldDirection * RAY_OFFSET_EPSILON), worldDirection);
  Colour indirectLight = pathTrace(newRay, newThroughput, depth + 1, pdf, sampler, shadingData.bsdf->isPureSpecular());

  return indirectLight + (directLight * throughput);
}
