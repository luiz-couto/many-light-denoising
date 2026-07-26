#include "light.h"

// Default stub. only needed for bidirectional methods.
// Lights that don't support it inherit this and return pdf=0 to signal "not available".
Vec3 Light::samplePositionFromLight(Sampler* sampler, float& pdf) {
	pdf = 0.0f;
	return Vec3(0.0f, 0.0f, 0.0f);
}

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

BackgroundColour::BackgroundColour(Colour _emission): emission(_emission) {}

Vec3 BackgroundColour::sample(const ShadingData& shadingData, Sampler* sampler, Colour& reflectedColour, float& pdf) {
  Vec3 wi = SamplingDistributions::uniformSampleSphere(sampler->next(), sampler->next());
  pdf = SamplingDistributions::uniformSpherePDF(wi);
  reflectedColour = emission;
  return wi;
}

Colour BackgroundColour::evaluate(const Vec3& wi) {
  return emission;
}

float BackgroundColour::PDF(const ShadingData& shadingData, const Vec3& wi) {
  return SamplingDistributions::uniformSpherePDF(wi);
}

bool BackgroundColour::isArea() {
  return false;
}

Vec3 BackgroundColour::normal(const ShadingData& shadingData, const Vec3& wi) {
  return -wi;
}

float BackgroundColour::totalIntegratedPower() {
  return emission.lum() * 4.0f * PI;
}

Vec3 BackgroundColour::sampleDirectionFromLight(Sampler* sampler, float& pdf) {
  Vec3 wi = SamplingDistributions::uniformSampleSphere(sampler->next(), sampler->next());
  pdf = SamplingDistributions::uniformSpherePDF(wi);
  return wi;
}
