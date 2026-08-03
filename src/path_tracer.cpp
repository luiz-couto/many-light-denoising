#include "path_tracer.h"
#include "core.h"
#include "geometry.h"

Colour PathTracerIntegrator::integrate(const Ray& ray, Sampler* sampler) {
  Colour start = Colour(1.0f, 1.0f, 1.0f);
  return pathTrace(ray, start, 0, 0.0f, sampler, false);
}

Colour PathTracerIntegrator::pathTrace(const Ray& ray, Colour throughput, int depth, float bsdfPDF, Sampler* sampler, bool isSpecularBounce) {
  if (depth > MAX_DEPTH) {
		return Colour(0.0f, 0.0f, 0.0f);
	}

  IntersectionData intersection = scene->traverse(ray);
	ShadingData shadingData = scene->calculateShadingData(intersection, ray);

  // miss
  if (shadingData.t == FLT_MAX) {
    return throughput * scene->background->evaluate(ray.dir);
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

  // RR
  if (depth >= RR_DEPTH) {
    float q = newThroughput.lum();
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
