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
  return 1.0f / (1.0f + lambdaGGX(wi, alpha) + lambdaGGX(wo, alpha));
}

float ShadingHelper::Dggx(Vec3 h, float alpha) {
  float cosThetaM = h.z;
  float alphaSqr = alpha * alpha;
  float term = (cosThetaM * cosThetaM) * (alphaSqr - 1) + 1;

  float D = alphaSqr / (M_PI * (term * term));
  return D;
}

// Reflects wo about the surface normal (z-axis in local space): flips the tangential
// components and preserves the normal component.
Vec3 ShadingHelper::reflect(Vec3 wo_local) {
  return Vec3(-wo_local.x, -wo_local.y, wo_local.z);
}

// Computes the Snell's law refracted direction. The tangential components scale by
// eta = n_i/n_t (Snell: n_i·sin_i = n_t·sin_t); the normal component flips sign to
// send the ray through the surface. Falls back to reflection on TIR (sets tir=true).
Vec3 ShadingHelper::refract(Vec3 wo_local, float n, bool& tir) {
  // n = n_t/n_i; entering when wo_local.z > 0, exiting when wo_local.z < 0
  float cosTheta = fabsf(wo_local.z);
  float cosThetaT = getCosThetaT(cosTheta, n, tir);

  if (tir) return reflect(wo_local);

  float eta = 1.0f / n; // n_i/n_t for the Snell's law direction formula
  float sign = (wo_local.z >= 0.0f) ? 1.0f : -1.0f;

  return Vec3(-eta * wo_local.x, -eta * wo_local.y, -cosThetaT * sign);
}

DiffuseBSDF::DiffuseBSDF(Texture* _albedo): albedo(_albedo) {}

// Cosine-weighted hemisphere sampling. PDF = cos(θ)/π matches the cos(θ) in the
// rendering equation, so the MC weight (albedo/π)·cos(θ)/(cos(θ)/π) simplifies to
// albedo — no division by π needed in reflectedColour.
Vec3 DiffuseBSDF::sample(const ShadingData& shadingData, Sampler* sampler, Colour& reflectedColour, float& pdf) {
  Vec3 wi = SamplingDistributions::cosineSampleHemisphere(sampler->next(), sampler->next());
  reflectedColour = albedo->sample(shadingData.tu, shadingData.tv);
  wi = shadingData.frame.toWorld(wi);
  pdf = PDF(shadingData, wi);
  return wi;
}

// Lambertian BRDF: f = albedo/π. The 1/π factor ensures energy conservation —
// integrating f·cos(θ) over the hemisphere gives albedo (≤ 1).
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

MirrorBSDF::MirrorBSDF(Texture* _albedo, Colour _eta, Colour _k)
  : albedo(_albedo), eta(_eta), k(_k) {}

// Perfect specular reflection. The BRDF is a delta distribution, so only one direction
// contributes: pdf = 1, and the MC weight f·cos(θ)/pdf = F(cos(θ))·albedo.
Vec3 MirrorBSDF::sample(const ShadingData& shadingData, Sampler* sampler, Colour& reflectedColour, float& pdf) {
  Vec3 woLocal = shadingData.frame.toLocal(shadingData.wo);
  Vec3 wiLocal(-woLocal.x, -woLocal.y, woLocal.z);
  float cosTheta = fabsf(wiLocal.z);
  reflectedColour = ShadingHelper::fresnelConductor(cosTheta, eta, k) * albedo->sample(shadingData.tu, shadingData.tv);
  pdf = 1.0f;
  return shadingData.frame.toWorld(wiLocal);
}

// The delta BRDF is f = F·albedo·δ(wi − wr)/cos(θ). For the exact mirror direction
// f·cos(θ) = F·albedo, so f = F·albedo/cos(θ). Used by MIS to weight light samples.
Colour MirrorBSDF::evaluate(const ShadingData& shadingData, const Vec3& wi) {
  Vec3 localWi = shadingData.frame.toLocal(wi);
  float cosTheta = fabsf(localWi.z);
  return ShadingHelper::fresnelConductor(cosTheta, eta, k) * albedo->sample(shadingData.tu, shadingData.tv) / cosTheta;
}

float MirrorBSDF::PDF(const ShadingData& shadingData, const Vec3& wi) {
  return 0;
}

bool MirrorBSDF::isPureSpecular() {
  return true;
}

bool MirrorBSDF::isTwoSided() {
  return true;
}

float MirrorBSDF::mask(const ShadingData& shadingData) {
  return albedo->sampleAlpha(shadingData.tu, shadingData.tv);
}

ConductorBSDF::ConductorBSDF(Texture* _albedo, Colour _eta, Colour _k, float roughness)
  : albedo(_albedo), eta(_eta), k(_k) {
    alpha = roughness * roughness;
  }

// GGX NDF importance sampling. A microfacet normal wm is drawn from D(wm), then wo is
// reflected about wm to get wi. The PDF in solid angle measure is D(wm)·cos(θm) / (4·dot(wo,wm)),
// which is the Jacobian of the half-vector mapping from microfacet normals to outgoing directions.
// Samples whose reflected wi goes below the surface are discarded (pdf = 0, black weight).
Vec3 ConductorBSDF::sample(const ShadingData& shadingData, Sampler* sampler, Colour& reflectedColour, float& pdf) {
  float s1 = sampler->next();
  float s2 = sampler->next();

  float thetaM = (1 - s1) / ((s1 * ((alpha * alpha) - 1)) + 1);
  thetaM = acosf(sqrtf(thetaM));
  float phiM = 2 * M_PI * s2;

  Vec3 woLocal = shadingData.frame.toLocal(shadingData.wo);

  Vec3 wm = Vec3(sinf(thetaM) * cosf(phiM), sinf(thetaM) * sinf(phiM), cosf(thetaM));
  Vec3 wi = -woLocal + (wm * (2 * wm.dot(woLocal)));
  Vec3 wiWorld = shadingData.frame.toWorld(wi);

  // Reflected direction can go below the surface for steep microfacets, so null sample
  if (wi.z <= 0.0f || wm.dot(woLocal) <= 0.0f) {
    pdf = 0.0f;
    reflectedColour = Colour(0.0f, 0.0f, 0.0f);
    return wiWorld;
  }

  pdf = (ShadingHelper::Dggx(wm, alpha) * cosf(thetaM)) / (4 * wm.dot(woLocal));
  reflectedColour = evaluate(shadingData, wiWorld) * wi.z / pdf;
  return wiWorld;
}

// Cook-Torrance microfacet BRDF: f = F(dot(wo,h)) · G(wi,wo) · D(h) / (4·cos(θo)·cos(θi)).
// F is the conductor Fresnel evaluated at the half-vector angle (not the surface normal angle).
// G is the Smith height-correlated masking-shadowing term. D is the GGX normal distribution.
Colour ConductorBSDF::evaluate(const ShadingData& shadingData, const Vec3& wi) {
  Vec3 localWi = shadingData.frame.toLocal(wi);
  Vec3 localWo = shadingData.frame.toLocal(shadingData.wo);

  Vec3 h = (localWi + localWo).normalize();
  float gWoWi = ShadingHelper::Gggx(localWi, localWo, alpha);
  float dWm = ShadingHelper::Dggx(h, alpha);
  float halfVecAngle = localWo.dot(h);
  Colour fresnel = ShadingHelper::fresnelConductor(halfVecAngle, eta, k);

  Colour term1 = fresnel * gWoWi * dWm;
  float term2 = 4 * localWo.z * localWi.z;
  Colour brdf = term1 / term2;

  return brdf * albedo->sample(shadingData.tu, shadingData.tv);
}

float ConductorBSDF::PDF(const ShadingData& shadingData, const Vec3& wi) {
  Vec3 localWi = shadingData.frame.toLocal(wi);
  Vec3 localWo = shadingData.frame.toLocal(shadingData.wo);

  Vec3 h = (localWi + localWo).normalize();
  float woDotH = localWo.dot(h);
  if (woDotH <= 0.0f) return 0.0f;
  
  float dWm = ShadingHelper::Dggx(h, alpha);
  float cosThetaM = h.z;

  float pdf = (dWm * cosThetaM) / (4 * woDotH);
  return pdf;
}

bool ConductorBSDF::isPureSpecular() {
  return false;
}

bool ConductorBSDF::isTwoSided() {
  return true;
}

float ConductorBSDF::mask(const ShadingData& shadingData) {
  return albedo->sampleAlpha(shadingData.tu, shadingData.tv);
}

GlassBSDF::GlassBSDF(Texture* _albedo, float _intIOR, float _extIOR)
  : albedo(_albedo), intIOR(_intIOR), extIOR(_extIOR) {}

// Stochastic Fresnel sampling: reflect with probability F, refract with probability 1-F.
// The pdf equals the chosen branch probability, so the MC weight f·cos(θ)/pdf simplifies
// to albedo in both branches. TIR forces reflection (F = 1, pdf = 1).
Vec3 GlassBSDF::sample(const ShadingData& shadingData, Sampler* sampler, Colour& reflectedColour, float& pdf) {
  Vec3 woLocal = shadingData.frame.toLocal(shadingData.wo);

  bool isEntering = woLocal.z > 0.0f;
  float n = isEntering ? (intIOR / extIOR) : (extIOR / intIOR);

  float cosTheta = fabsf(woLocal.z);
  Colour albedoVal = albedo->sample(shadingData.tu, shadingData.tv);

  bool tir;
  Vec3 wtLocal = ShadingHelper::refract(woLocal, n, tir);
  float fresnel = tir ? 1.0f : ShadingHelper::fresnelDielectric(cosTheta, n);

  reflectedColour = albedoVal; // weight = albedo in both branches (Fresnel prob cancels with pdf)

  if (sampler->next() < fresnel) {
    pdf = fresnel;
    return shadingData.frame.toWorld(ShadingHelper::reflect(woLocal));
  }

  pdf = 1.0f - fresnel;
  return shadingData.frame.toWorld(wtLocal);
}

// Delta BSDF: the exact reflect/refract directions have measure zero and cannot be
// hit by an arbitrary wi, so evaluate always returns zero. Contribution comes only
// from sample(), not from direct-lighting or MIS calls to evaluate().
Colour GlassBSDF::evaluate(const ShadingData&, const Vec3&) {
  return Colour(0.0f, 0.0f, 0.0f);
}

float GlassBSDF::PDF(const ShadingData& shadingData, const Vec3& wi) {
  return 0;
}

bool GlassBSDF::isPureSpecular() {
  return true;
}

bool GlassBSDF::isTwoSided() {
  return false;
}

float GlassBSDF::mask(const ShadingData& shadingData) {
  return albedo->sampleAlpha(shadingData.tu, shadingData.tv);
}

PlasticBSDF::PlasticBSDF(Texture* _albedo, float _intIOR, float _extIOR, float roughness)
  : albedo(_albedo), intIOR(_intIOR), extIOR(_extIOR) {
    alpha = roughness * roughness;
  }

// Mixture sampling: with probability F sample the GGX specular lobe, otherwise cosine-sample
// the diffuse lobe. The pdf is the full mixture F·pdfGGX + (1-F)·pdfCosine regardless of which
// branch was chosen, so the weight f·cosθ/pdf is computed via evaluate() rather than simplified.
Vec3 PlasticBSDF::sample(const ShadingData& shadingData, Sampler* sampler, Colour& reflectedColour, float& pdf) {
  Vec3 woLocal = shadingData.frame.toLocal(shadingData.wo);
  float fresnel = ShadingHelper::fresnelDielectric(woLocal.z, intIOR, extIOR);

  Vec3 wiLocal;
  if (sampler->next() < fresnel) {
    // GGX specular branch — same sampling as ConductorBSDF
    float s1 = sampler->next();
    float s2 = sampler->next();
    float thetaM = acosf(sqrtf((1 - s1) / ((s1 * ((alpha * alpha) - 1)) + 1)));
    float phiM = 2 * M_PI * s2;
    Vec3 wm = Vec3(sinf(thetaM) * cosf(phiM), sinf(thetaM) * sinf(phiM), cosf(thetaM));
    wiLocal = -woLocal + (wm * (2 * wm.dot(woLocal)));

    if (wiLocal.z <= 0.0f || wm.dot(woLocal) <= 0.0f) {
      pdf = 0.0f;
      reflectedColour = Colour(0.0f, 0.0f, 0.0f);
      return shadingData.frame.toWorld(wiLocal);
    }
  } else {
    // Diffuse branch — cosine-weighted hemisphere sampling
    wiLocal = SamplingDistributions::cosineSampleHemisphere(sampler->next(), sampler->next());
  }

  Vec3 wiWorld = shadingData.frame.toWorld(wiLocal);
  pdf = PDF(shadingData, wiWorld);
  if (pdf <= 0.0f) {
    reflectedColour = Colour(0.0f, 0.0f, 0.0f);
    return wiWorld;
  }
  reflectedColour = evaluate(shadingData, wiWorld) * wiLocal.z / pdf;
  return wiWorld;
}

Colour PlasticBSDF::evaluate(const ShadingData& shadingData, const Vec3& wi) {
  Vec3 localWi = shadingData.frame.toLocal(wi);
  Vec3 localWo = shadingData.frame.toLocal(shadingData.wo);

  if (localWi.z <= 0.0f || localWo.z <= 0.0f) return Colour(0.0f, 0.0f, 0.0f);

  Vec3 h = (localWi + localWo).normalize();
  float gWoWi = ShadingHelper::Gggx(localWi, localWo, alpha);
  float dWm = ShadingHelper::Dggx(h, alpha);
  float fresnel = ShadingHelper::fresnelDielectric(localWo.dot(h), intIOR, extIOR);

  float term1 = fresnel * gWoWi * dWm;
  float term2 = 4 * localWo.z * localWi.z;
  float brdf = term1 / term2;

  Colour albedoSample = albedo->sample(shadingData.tu, shadingData.tv);

  return Colour(brdf, brdf, brdf) + ((albedoSample * (1 - fresnel)) / PI);
}

// Mixture PDF: F·pdfGGX(wi) + (1-F)·pdfCosine(wi), where F is the dielectric Fresnel
// at the surface normal (the same branching probability used in sample).
float PlasticBSDF::PDF(const ShadingData& shadingData, const Vec3& wi) {
  Vec3 localWi = shadingData.frame.toLocal(wi);
  Vec3 localWo = shadingData.frame.toLocal(shadingData.wo);

  if (localWi.z <= 0.0f) return 0.0f;

  Vec3 h = (localWi + localWo).normalize();
  float woDotH = localWo.dot(h);
  if (woDotH <= 0.0f) return 0.0f;

  float fresnel = ShadingHelper::fresnelDielectric(localWo.z, intIOR, extIOR);
  float pdfGGX = (ShadingHelper::Dggx(h, alpha) * h.z) / (4.0f * woDotH);
  float pdfCosine = localWi.z / M_PI;

  return fresnel * pdfGGX + (1.0f - fresnel) * pdfCosine;
}

bool PlasticBSDF::isPureSpecular() {
  return false;
}

bool PlasticBSDF::isTwoSided() {
  return true;
}

float PlasticBSDF::mask(const ShadingData& shadingData) {
  return albedo->sampleAlpha(shadingData.tu, shadingData.tv);
}
