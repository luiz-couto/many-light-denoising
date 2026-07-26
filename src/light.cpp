#include "light.h"

Vec3 AreaLight::sample(const ShadingData& shadingData, Sampler* sampler, Colour& emittedColour, float& pdf) {
  emittedColour = emission;
	return triangle->sample(sampler, pdf);
}

Colour AreaLight::evaluate(const Vec3& wi) {
  if (dot(wi, triangle->gNormal()) < 0) return emission;
  return Colour(0.0f, 0.0f, 0.0f);
}

// Returns the pdf in area measure (1/area), consistent with sample().
// The integrator must convert to solid-angle measure for MIS:
//   pdf_sa = pdf_area * dist² / dot(-wi, light_normal)
// where dist is the distance from the shading point to the sampled light position.
float AreaLight::PDF(const ShadingData& shadingData, const Vec3& wi) {
  return 1.0f / triangle->area;
}

bool AreaLight::isArea() {
  return true;
}

Vec3 AreaLight::normal(const ShadingData& shadingData, const Vec3& wi) {
  return triangle->gNormal();
}

float AreaLight::totalIntegratedPower() {
  return (triangle->area * emission.lum() * M_PI);
}

Vec3 AreaLight::samplePositionFromLight(Sampler* sampler, float& pdf) {
  return triangle->sample(sampler, pdf);
}

Vec3 AreaLight::sampleDirectionFromLight(Sampler* sampler, float& pdf) {
  Vec3 wi = SamplingDistributions::cosineSampleHemisphere(sampler->next(), sampler->next());
  pdf = SamplingDistributions::cosineHemispherePDF(wi);

  Frame frame;
  frame.fromVector(triangle->gNormal());
  return frame.toWorld(wi);
}
