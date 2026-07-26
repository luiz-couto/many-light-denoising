#include "shading.h"

bool BSDF::isLight() {
  return emission.lum() > 0;
}

void BSDF::addLight(Colour emission) {
  this->emission = emission;
}

Colour BSDF::emit(const ShadingData& shadingData, const Vec3& wi) {
  return emission;
}

float ShadingHelper::getCosThetaT(float cosTheta, float n, bool &totalInternalReflection) {
  totalInternalReflection = false;
  float term = 1 - ((1 - (cosTheta * cosTheta)) / (n * n));
  if (term < 0) {
    totalInternalReflection = true;
    return 0.0f;
  }
  return sqrtf(term);
}

float ShadingHelper::fresnelDielectric(float cosTheta, float intIOR, float extIOR) {
  bool isEntering = cosTheta > 0;
  float n = isEntering ? (intIOR / extIOR) : (extIOR / intIOR);

  float cosThetaAbs = fabsf(cosTheta);

  bool totalInternalReflection;
  float cosThetaT = getCosThetaT(cosThetaAbs, n, totalInternalReflection);
  if (totalInternalReflection) {
    return 1.0f;
  }

  // Fresnel amplitude coefficients for s (perpendicular) and p (parallel) polarisation
  float perpendicularAmplitude = (cosThetaAbs - (n * cosThetaT)) / (cosThetaAbs + (n * cosThetaT));
  float parallelAmplitude = ((n * cosThetaAbs) - cosThetaT) / ((n * cosThetaAbs) + cosThetaT);

  return ((perpendicularAmplitude * perpendicularAmplitude) + (parallelAmplitude * parallelAmplitude)) / 2.0f;
}

float ShadingHelper::fresnelDielectric(float cosTheta, float n) {
  float cosThetaAbs = fabsf(cosTheta);
  bool tir;
  float cosThetaT = getCosThetaT(cosThetaAbs, n, tir);
  if (tir) return 1.0f;

  // Fresnel amplitude coefficients for s (perpendicular) and p (parallel) polarisation
  float perpendicularAmplitude = (cosThetaAbs - (n * cosThetaT)) / (cosThetaAbs + (n * cosThetaT));
  float parallelAmplitude = ((n * cosThetaAbs) - cosThetaT) / ((n * cosThetaAbs) + cosThetaT);

  return ((perpendicularAmplitude * perpendicularAmplitude) + (parallelAmplitude * parallelAmplitude)) / 2.0f;
}

float ShadingHelper::fresnelConductorPerpendicularSqr(float cosTheta, float n, float k) {
  float cosThetaSqr = (cosTheta * cosTheta);
  float term1 = ((n * n) + (k * k)) - (2 * n * cosTheta) + cosThetaSqr;
  float term2 = ((n * n) + (k * k)) + (2 * n * cosTheta) + cosThetaSqr;
  return term1 / term2;
}

float ShadingHelper::fresnelConductorParallelSqr(float cosTheta, float n, float k) {
  float cosThetaSqr = (cosTheta * cosTheta);
  float term1 = (((n * n) + (k * k)) * cosThetaSqr) - (2 * n * cosTheta) + 1.0f;
  float term2 = (((n * n) + (k * k)) * cosThetaSqr) + (2 * n * cosTheta) + 1.0f;
  return term1 / term2;
}

Colour ShadingHelper::fresnelConductor(float cosTheta, Colour ior, Colour k) {
  float r = (fresnelConductorParallelSqr(cosTheta, ior.r, k.r) + fresnelConductorPerpendicularSqr(cosTheta, ior.r, k.r)) / 2;
  float g = (fresnelConductorParallelSqr(cosTheta, ior.g, k.g) + fresnelConductorPerpendicularSqr(cosTheta, ior.g, k.g)) / 2;
  float b = (fresnelConductorParallelSqr(cosTheta, ior.b, k.b) + fresnelConductorPerpendicularSqr(cosTheta, ior.b, k.b)) / 2;
  return Colour(r, g, b);
}

float ShadingHelper::lambdaGGX(Vec3 wi, float alpha) {
  float cosTheta = std::max(fabsf(wi.z), 1e-6f); // clamp to avoid /0; grazing gives large lambda → G≈0
  float cosThetaSqr = cosTheta * cosTheta;
  float tanThetaSqr = (1.0f - cosThetaSqr) / cosThetaSqr;

  float term = 1.0f + (alpha * alpha * tanThetaSqr);
  return (sqrtf(term) - 1.0f) / 2.0f;
}

float ShadingHelper::Gggx(Vec3 wi, Vec3 wo, float alpha) {
  float go = 1 / (1 + lambdaGGX(wo, alpha));
  float gi = 1 / (1 + lambdaGGX(wi, alpha));

  return go * gi;
}

float ShadingHelper::Dggx(Vec3 h, float alpha) {
  float cosThetaM = h.z;
  float alphaSqr = alpha * alpha;
  float term = (cosThetaM * cosThetaM) * (alphaSqr - 1) + 1;

  float D = alphaSqr / (M_PI * (term * term));
  return D;
}

DiffuseBSDF::DiffuseBSDF(Texture* _albedo): albedo(_albedo) {}

Vec3 DiffuseBSDF::sample(const ShadingData& shadingData, Sampler* sampler, Colour& reflectedColour, float& pdf) {
  Vec3 wi = SamplingDistributions::cosineSampleHemisphere(sampler->next(), sampler->next());
  reflectedColour = albedo->sample(shadingData.tu, shadingData.tv);
  wi = shadingData.frame.toWorld(wi);
  pdf = PDF(shadingData, wi);
  return wi;
}

Colour DiffuseBSDF::evaluate(const ShadingData& shadingData, const Vec3& wi) {
  return albedo->sample(shadingData.tu, shadingData.tv) / PI;
}

float DiffuseBSDF::PDF(const ShadingData& shadingData, const Vec3& wi) {
  Vec3 localWi = shadingData.frame.toLocal(wi);
  return SamplingDistributions::cosineHemispherePDF(localWi);
}

bool DiffuseBSDF::isPureSpecular() {
  return false;
}

bool DiffuseBSDF::isTwoSided() {
  return true;
}

float DiffuseBSDF::mask(const ShadingData& shadingData) {
  return albedo->sampleAlpha(shadingData.tu, shadingData.tv);
}
