#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "shading.h"
#include "texture.h"
#include "sampling.h"
#include "bsdf_test_utils.h"
#include <cmath>

static Texture makeWhiteTex() { Texture t; t.loadDefault(); return t; }

static ShadingData makeSD(Vec3 wo) {
  ShadingData sd(Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f));
  sd.wo = wo;
  sd.tu = sd.tv = 0.0f;
  return sd;
}

// Gold-like IOR (uniform across RGB for predictable, symmetric Fresnel)
static const Colour GOLD_ETA(0.37f, 0.37f, 0.37f);
static const Colour GOLD_K(2.82f, 2.82f, 2.82f);

// -----------------------------------------------------------------------
// Roughness remapping — alpha = roughness² (Disney remap)
// -----------------------------------------------------------------------

TEST_CASE("ConductorBSDF roughness remap: roughness=0 clamped to MIN_ALPHA") {
  // roughness=0 → roughness²=0 < MIN_ALPHA=0.01 → clamped to prevent near-delta GGX
  Texture tex = makeWhiteTex();
  ConductorBSDF bsdf(&tex, GOLD_ETA, GOLD_K, 0.0f);
  REQUIRE(bsdf.alpha == Catch::Approx(0.01f));
}

TEST_CASE("ConductorBSDF roughness remap: roughness² below MIN_ALPHA clamped to MIN_ALPHA") {
  Texture tex = makeWhiteTex();
  // roughness=0.05 → roughness²=0.0025 < MIN_ALPHA=0.01 → clamped
  ConductorBSDF bsdf05(&tex, GOLD_ETA, GOLD_K, 0.05f);
  REQUIRE(bsdf05.alpha == Catch::Approx(0.01f));
  // roughness=0.09 → roughness²=0.0081 < MIN_ALPHA=0.01 → clamped
  ConductorBSDF bsdf09(&tex, GOLD_ETA, GOLD_K, 0.09f);
  REQUIRE(bsdf09.alpha == Catch::Approx(0.01f));
  // roughness=0.1 → roughness²=0.01 = MIN_ALPHA → not clamped (exact boundary)
  ConductorBSDF bsdf10(&tex, GOLD_ETA, GOLD_K, 0.1f);
  REQUIRE(bsdf10.alpha == Catch::Approx(0.01f));
}

TEST_CASE("ConductorBSDF roughness remap: roughness=1 gives alpha=1") {
  Texture tex = makeWhiteTex();
  ConductorBSDF bsdf(&tex, GOLD_ETA, GOLD_K, 1.0f);
  REQUIRE(bsdf.alpha == Catch::Approx(1.0f));
}

TEST_CASE("ConductorBSDF roughness remap: roughness=0.5 gives alpha=0.25") {
  Texture tex = makeWhiteTex();
  ConductorBSDF bsdf(&tex, GOLD_ETA, GOLD_K, 0.5f);
  REQUIRE(bsdf.alpha == Catch::Approx(0.25f));
}

TEST_CASE("ConductorBSDF roughness remap: alpha equals roughness squared") {
  Texture tex = makeWhiteTex();
  for (float r : {0.2f, 0.4f, 0.6f, 0.7f, 0.9f}) {
    ConductorBSDF bsdf(&tex, GOLD_ETA, GOLD_K, r);
    REQUIRE(bsdf.alpha == Catch::Approx(r * r).margin(0.0001f));
  }
}

// -----------------------------------------------------------------------
// evaluate — non-negative for all valid upper-hemisphere directions
// -----------------------------------------------------------------------

TEST_CASE("ConductorBSDF evaluate: non-negative for many wi/wo combinations") {
  Texture tex = makeWhiteTex();
  ConductorBSDF bsdf(&tex, GOLD_ETA, GOLD_K, 0.5f);

  float cosines[] = {0.99f, 0.8f, 0.6f, 0.4f, 0.2f};
  for (float cwo : cosines) {
    for (float cwi : cosines) {
      ShadingData sd = makeSD(Vec3(sqrtf(1.0f - cwo * cwo), 0.0f, cwo));
      Colour result = bsdf.evaluate(sd, Vec3(sqrtf(1.0f - cwi * cwi), 0.0f, cwi));
      REQUIRE(result.r >= 0.0f);
      REQUIRE(result.g >= 0.0f);
      REQUIRE(result.b >= 0.0f);
    }
  }
}

// -----------------------------------------------------------------------
// evaluate — scales linearly with albedo
// -----------------------------------------------------------------------

TEST_CASE("ConductorBSDF evaluate: scales linearly with albedo") {
  Texture whiteTex = makeWhiteTex();

  Texture greyTex;
  greyTex.width = greyTex.height = greyTex.channels = 1;
  greyTex.texels = new Colour[1];
  greyTex.texels[0] = Colour(0.5f, 0.5f, 0.5f);

  ConductorBSDF white_bsdf(&whiteTex, GOLD_ETA, GOLD_K, 0.5f);
  ConductorBSDF grey_bsdf(&greyTex, GOLD_ETA, GOLD_K, 0.5f);

  Vec3 wo(0.0f, 0.0f, 1.0f), wi(0.5f, 0.0f, sqrtf(0.75f));
  Colour r_white = white_bsdf.evaluate(makeSD(wo), wi);
  Colour r_grey  = grey_bsdf.evaluate(makeSD(wo), wi);

  REQUIRE(r_grey.r == Catch::Approx(r_white.r * 0.5f).margin(0.001f));
  REQUIRE(r_grey.g == Catch::Approx(r_white.g * 0.5f).margin(0.001f));
  REQUIRE(r_grey.b == Catch::Approx(r_white.b * 0.5f).margin(0.001f));
}

// -----------------------------------------------------------------------
// evaluate — Helmholtz reciprocity (also validates the Fresnel half-vector fix)
//
// With the old code (Fresnel(wo.z)), swapping wi and wo changes wo.z so
// f(wi,wo) ≠ f(wo,wi). With the fix (Fresnel(dot(wo,h))), dot(wi,h)=dot(wo,h)
// always, so the BRDF is symmetric.
// -----------------------------------------------------------------------

TEST_CASE("ConductorBSDF evaluate: Helmholtz reciprocal f(wi,wo) = f(wo,wi)") {
  Texture tex = makeWhiteTex();
  ConductorBSDF bsdf(&tex, GOLD_ETA, GOLD_K, 0.5f);

  Vec3 wo_A(0.0f, 0.0f, 1.0f);
  Vec3 wi_B(0.5f, 0.0f, sqrtf(0.75f));

  Colour f_AB = bsdf.evaluate(makeSD(wo_A), wi_B); // f(wi=B, wo=A)
  Colour f_BA = bsdf.evaluate(makeSD(wi_B), wo_A); // f(wi=A, wo=B)

  REQUIRE(f_AB.r == Catch::Approx(f_BA.r).margin(0.001f));
  REQUIRE(f_AB.g == Catch::Approx(f_BA.g).margin(0.001f));
  REQUIRE(f_AB.b == Catch::Approx(f_BA.b).margin(0.001f));
}

TEST_CASE("ConductorBSDF evaluate: reciprocal for oblique non-symmetric pair") {
  Texture tex = makeWhiteTex();
  ConductorBSDF bsdf(&tex, GOLD_ETA, GOLD_K, 0.7f);

  Vec3 wo_A(0.6f, 0.0f, 0.8f); // cos=0.8
  Vec3 wi_B(0.0f, 0.7f, sqrtf(1.0f - 0.49f)); // different azimuth

  Colour f_AB = bsdf.evaluate(makeSD(wo_A), wi_B);
  Colour f_BA = bsdf.evaluate(makeSD(wi_B), wo_A);

  REQUIRE(f_AB.r == Catch::Approx(f_BA.r).margin(0.001f));
}

// -----------------------------------------------------------------------
// evaluate — explicit formula at normal backscatter
//
// wo = wi = (0,0,1): h=(0,0,1), G=1 (lambdaGGX=0 at cosTheta=1),
// D = alpha²/(π*(alpha²)²) = 1/(π*alpha²), F = fresnelConductor(1, eta, k)
// BRDF = F * 1 * D / (4 * 1 * 1)
// -----------------------------------------------------------------------

TEST_CASE("ConductorBSDF evaluate: matches F*G*D/4 at normal-incidence backscatter") {
  Texture tex = makeWhiteTex();
  float roughness = sqrtf(0.5f); // alpha = 0.5
  ConductorBSDF bsdf(&tex, GOLD_ETA, GOLD_K, roughness);

  Vec3 normal(0.0f, 0.0f, 1.0f);
  Colour result = bsdf.evaluate(makeSD(normal), normal);

  float alpha = roughness * roughness;
  Colour F = ShadingHelper::fresnelConductor(1.0f, GOLD_ETA, GOLD_K); // dot(wo,h)=1 at normal incidence
  float D = ShadingHelper::Dggx(Vec3(0.0f, 0.0f, 1.0f), alpha);
  // G = 1 because lambdaGGX(wo=(0,0,1)) = 0 at zero tangential component

  REQUIRE(result.r == Catch::Approx(F.r * D / 4.0f).margin(0.005f));
  REQUIRE(result.g == Catch::Approx(F.g * D / 4.0f).margin(0.005f));
  REQUIRE(result.b == Catch::Approx(F.b * D / 4.0f).margin(0.005f));
}

// -----------------------------------------------------------------------
// evaluate — maximally rough surface (roughness=1, alpha=1)
// At wo=wi=(0,0,1): D=1/π, G=1, F given by GOLD, BRDF = F/(4π) ≈ 0.0676
// -----------------------------------------------------------------------

TEST_CASE("ConductorBSDF evaluate: maximally rough matches F*G*D/4 at backscatter") {
  Texture tex = makeWhiteTex();
  ConductorBSDF bsdf(&tex, GOLD_ETA, GOLD_K, 1.0f); // alpha=1

  Vec3 normal(0.0f, 0.0f, 1.0f);
  Colour result = bsdf.evaluate(makeSD(normal), normal);

  Colour F = ShadingHelper::fresnelConductor(1.0f, GOLD_ETA, GOLD_K);
  float D = ShadingHelper::Dggx(Vec3(0.0f, 0.0f, 1.0f), 1.0f); // = 1/π

  REQUIRE(result.r == Catch::Approx(F.r * D / 4.0f).margin(0.002f));
  REQUIRE(std::isfinite(result.r));
}

// -----------------------------------------------------------------------
// evaluate — perfect reflector: k→∞ means F≈1, BRDF ≈ G*D/4
// -----------------------------------------------------------------------

TEST_CASE("ConductorBSDF evaluate: perfect reflector (k=1000) backscatter = G*D/4") {
  // F → 1 for all angles when k is large.
  // At wo=wi=(0,0,1), alpha=1: BRDF = 1 * 1 * (1/π) / 4 = 1/(4π)
  Texture tex = makeWhiteTex();
  ConductorBSDF bsdf(&tex, Colour(1.0f, 1.0f, 1.0f), Colour(1000.0f, 1000.0f, 1000.0f), 1.0f);

  Vec3 normal(0.0f, 0.0f, 1.0f);
  Colour result = bsdf.evaluate(makeSD(normal), normal);

  float expected = 1.0f / (4.0f * M_PI);
  REQUIRE(result.r == Catch::Approx(expected).margin(0.002f));
}

// -----------------------------------------------------------------------
// evaluate — near-grazing angles stay finite and non-negative
// -----------------------------------------------------------------------

TEST_CASE("ConductorBSDF evaluate: near-grazing angles are finite and non-negative") {
  Texture tex = makeWhiteTex();
  ConductorBSDF bsdf(&tex, GOLD_ETA, GOLD_K, 0.5f);

  float cwo = 0.05f; // ~87° from normal
  ShadingData sd = makeSD(Vec3(sqrtf(1.0f - cwo * cwo), 0.0f, cwo));

  float cwi[] = {0.9f, 0.6f, 0.3f, 0.05f};
  for (float c : cwi) {
    Colour result = bsdf.evaluate(sd, Vec3(sqrtf(1.0f - c * c), 0.0f, c));
    REQUIRE(std::isfinite(result.r));
    REQUIRE(result.r >= 0.0f);
  }
}

// -----------------------------------------------------------------------
// PDF — non-negative
// -----------------------------------------------------------------------

TEST_CASE("ConductorBSDF PDF: non-negative for valid upper-hemisphere wi") {
  Texture tex = makeWhiteTex();
  ConductorBSDF bsdf(&tex, GOLD_ETA, GOLD_K, 0.5f);
  ShadingData sd = makeSD(Vec3(0.0f, 0.0f, 1.0f));

  float cosines[] = {0.99f, 0.8f, 0.6f, 0.4f, 0.2f, 0.05f};
  for (float c : cosines) {
    Vec3 wi(sqrtf(1.0f - c * c), 0.0f, c);
    REQUIRE(bsdf.PDF(sd, wi) >= 0.0f);
  }
}

// -----------------------------------------------------------------------
// PDF — explicit formula check
//
// wo=(0,0,1), wi=(0.5,0,√0.75), roughness=√0.5 → alpha=0.5:
// h = normalize((0.5, 0, 1+√0.75)) ≈ (0.259, 0, 0.966)
// D(h,0.5) ≈ 0.8826, h.z ≈ 0.966, dot(wo,h) = h.z (since wo=(0,0,1))
// PDF = D * h.z / (4 * h.z) = D / 4 ≈ 0.2207
// -----------------------------------------------------------------------

TEST_CASE("ConductorBSDF PDF: matches D(h)*h.z/(4*dot(wo,h)) for known geometry") {
  Texture tex = makeWhiteTex();
  float roughness = sqrtf(0.5f); // alpha = 0.5
  ConductorBSDF bsdf(&tex, GOLD_ETA, GOLD_K, roughness);

  Vec3 wo(0.0f, 0.0f, 1.0f);
  Vec3 wi(0.5f, 0.0f, sqrtf(0.75f));
  ShadingData sd = makeSD(wo);

  float pdf = bsdf.PDF(sd, wi);

  // Independently compute expected using the same helper functions
  Vec3 localWo = sd.frame.toLocal(wo);
  Vec3 localWi = sd.frame.toLocal(wi);
  Vec3 h = (localWo + localWi).normalize();
  float alpha = roughness * roughness;
  float D = ShadingHelper::Dggx(h, alpha);
  float woDotH = localWo.dot(h);
  float expected = D * h.z / (4.0f * woDotH);

  REQUIRE(pdf == Catch::Approx(expected).margin(0.001f));
  REQUIRE(pdf == Catch::Approx(0.2207f).margin(0.005f)); // sanity-check against hand-computed value
}

// -----------------------------------------------------------------------
// PDF — consistency: PDF(wi) matches the pdf returned by sample() for same wi
// -----------------------------------------------------------------------

TEST_CASE("ConductorBSDF PDF: matches sample pdf for valid sampled directions") {
  Texture tex = makeWhiteTex();
  ConductorBSDF bsdf(&tex, GOLD_ETA, GOLD_K, 0.5f);
  ShadingData sd = makeSD(Vec3(0.0f, 0.0f, 1.0f));
  MTRandom sampler;

  for (int i = 0; i < 30; i++) {
    Colour weight; float pdf_sample;
    Vec3 wi = bsdf.sample(sd, &sampler, weight, pdf_sample);
    if (pdf_sample > 0.0f) { // skip null samples (below-horizon wi)
      float pdf_eval = bsdf.PDF(sd, wi);
      REQUIRE(pdf_sample == Catch::Approx(pdf_eval).margin(0.001f));
    }
  }
}

// -----------------------------------------------------------------------
// sample — direction correctness
// -----------------------------------------------------------------------

TEST_CASE("ConductorBSDF sample: returned wi is in upper hemisphere when pdf > 0") {
  // GGX sampling can produce below-horizon wi for steep microfacets (null sample, pdf=0).
  // When pdf > 0 the sample is valid and wi must be in the upper hemisphere.
  Texture tex = makeWhiteTex();
  ConductorBSDF bsdf(&tex, GOLD_ETA, GOLD_K, 0.5f);
  ShadingData sd = makeSD(Vec3(0.0f, 0.0f, 1.0f));
  MTRandom sampler;

  for (int i = 0; i < 50; i++) {
    Colour weight; float pdf;
    Vec3 wi = bsdf.sample(sd, &sampler, weight, pdf);
    if (pdf > 0.0f) {
      Vec3 localWi = sd.frame.toLocal(wi);
      REQUIRE(localWi.z >= -0.001f);
    }
  }
}

// -----------------------------------------------------------------------
// sample — pdf
// -----------------------------------------------------------------------

TEST_CASE("ConductorBSDF sample: pdf is non-negative") {
  Texture tex = makeWhiteTex();
  ConductorBSDF bsdf(&tex, GOLD_ETA, GOLD_K, 0.5f);
  ShadingData sd = makeSD(Vec3(0.0f, 0.0f, 1.0f));
  MTRandom sampler;

  for (int i = 0; i < 50; i++) {
    Colour weight; float pdf;
    bsdf.sample(sd, &sampler, weight, pdf);
    REQUIRE(pdf >= 0.0f);
  }
}

// -----------------------------------------------------------------------
// sample — reflectedColour = evaluate(wi)
// -----------------------------------------------------------------------

TEST_CASE("ConductorBSDF sample: reflectedColour equals evaluate(wi) * cosWi / pdf for valid samples") {
  Texture tex = makeWhiteTex();
  ConductorBSDF bsdf(&tex, GOLD_ETA, GOLD_K, 0.5f);
  ShadingData sd = makeSD(Vec3(0.0f, 0.0f, 1.0f));
  MTRandom sampler;

  for (int i = 0; i < 20; i++) {
    Colour weight; float pdf;
    Vec3 wi = bsdf.sample(sd, &sampler, weight, pdf);
    if (pdf > 0.0f) {
      Colour eval = bsdf.evaluate(sd, wi);
      float cosWi = sd.frame.toLocal(wi).z;
      REQUIRE(weight.r == Catch::Approx(eval.r * cosWi / pdf).margin(0.001f));
      REQUIRE(weight.g == Catch::Approx(eval.g * cosWi / pdf).margin(0.001f));
      REQUIRE(weight.b == Catch::Approx(eval.b * cosWi / pdf).margin(0.001f));
    }
  }
}

TEST_CASE("ConductorBSDF sample: reflectedColour is non-negative") {
  Texture tex = makeWhiteTex();
  ConductorBSDF bsdf(&tex, GOLD_ETA, GOLD_K, 0.5f);
  ShadingData sd = makeSD(Vec3(0.0f, 0.0f, 1.0f));
  MTRandom sampler;

  for (int i = 0; i < 50; i++) {
    Colour weight; float pdf;
    bsdf.sample(sd, &sampler, weight, pdf);
    REQUIRE(weight.r >= 0.0f);
    REQUIRE(weight.g >= 0.0f);
    REQUIRE(weight.b >= 0.0f);
  }
}

// -----------------------------------------------------------------------
// sample — oblique incident angle still produces valid results
// -----------------------------------------------------------------------

TEST_CASE("ConductorBSDF sample: oblique wo gives valid pdf and non-negative weight") {
  Texture tex = makeWhiteTex();
  ConductorBSDF bsdf(&tex, GOLD_ETA, GOLD_K, 0.5f);
  float c = 0.6f;
  ShadingData sd = makeSD(Vec3(sqrtf(1.0f - c * c), 0.0f, c));
  MTRandom sampler;

  for (int i = 0; i < 30; i++) {
    Colour weight; float pdf;
    Vec3 wi = bsdf.sample(sd, &sampler, weight, pdf);
    REQUIRE(pdf >= 0.0f);
    REQUIRE(weight.r >= 0.0f);
    if (pdf > 0.0f) {
      Vec3 localWi = sd.frame.toLocal(wi);
      REQUIRE(localWi.z >= -0.001f);
    }
  }
}

// -----------------------------------------------------------------------
// isPureSpecular
// -----------------------------------------------------------------------

TEST_CASE("ConductorBSDF isPureSpecular: returns false (rough surface, not delta)") {
  Texture tex = makeWhiteTex();
  ConductorBSDF bsdf(&tex, GOLD_ETA, GOLD_K, 0.5f);
  REQUIRE(bsdf.isPureSpecular() == false);
}

// -----------------------------------------------------------------------
// evaluate vs roughness — rougher surface spreads energy, sharper peaks at low roughness
// -----------------------------------------------------------------------

TEST_CASE("ConductorBSDF evaluate: rougher surface has lower peak at retroreflection") {
  // At wo=wi=(0,0,1), D_rough < D_smooth, so BRDF decreases with roughness
  Texture tex = makeWhiteTex();
  ConductorBSDF smooth(&tex, GOLD_ETA, GOLD_K, 0.3f); // alpha=0.09
  ConductorBSDF rough (&tex, GOLD_ETA, GOLD_K, 0.8f); // alpha=0.64

  Vec3 normal(0.0f, 0.0f, 1.0f);
  Colour r_smooth = smooth.evaluate(makeSD(normal), normal);
  Colour r_rough  = rough.evaluate(makeSD(normal), normal);

  // Smoother surface concentrates more energy in the specular peak
  REQUIRE(r_smooth.r > r_rough.r);
}

// -----------------------------------------------------------------------
// Energy conservation
// -----------------------------------------------------------------------

TEST_CASE("ConductorBSDF energy conservation: reflectance <= 1 for white albedo") {
  Texture tex = makeWhiteTex();
  ConductorBSDF bsdf(&tex, GOLD_ETA, GOLD_K, 0.5f);
  float cosines[] = { 0.2f, 0.4f, 0.6f, 0.8f, 1.0f };
  for (float c : cosines) {
    float s = sqrtf(1.0f - c * c);
    ShadingData sd = makeTestSD(Vec3(s, 0.0f, c));
    float r = estimateReflectance(&bsdf, sd, 2000);
    REQUIRE(r <= 1.05f);
  }
}

// -----------------------------------------------------------------------
// Chi-square: sampled distribution matches PDF
// -----------------------------------------------------------------------

TEST_CASE("ConductorBSDF chi-square: sampled distribution matches PDF") {
  Texture tex = makeWhiteTex();
  // roughness=0.9 gives a broad lobe that distributes well across 16×16 bins;
  // roughness=0.5 produces a lobe too narrow for 16-bin discretisation to be accurate.
  ConductorBSDF bsdf(&tex, GOLD_ETA, GOLD_K, 0.9f);
  // 45° wo tests the asymmetric lobe well
  ShadingData sd = makeTestSD(Vec3(sqrtf(0.5f), 0.0f, sqrtf(0.5f)));
  REQUIRE(chiSquareTest(&bsdf, sd));
}

// -----------------------------------------------------------------------
// Roughness = 0 collapses to mirror
// -----------------------------------------------------------------------

TEST_CASE("ConductorBSDF roughness=0 clamped: samples concentrate near mirror direction") {
  // roughness=0 is clamped to MIN_ALPHA=0.01 — a very narrow (~0.6°) GGX lobe.
  // Samples are NOT exactly on the mirror direction (no longer a delta), but very close.
  Texture tex = makeWhiteTex();
  ConductorBSDF bsdf(&tex, GOLD_ETA, GOLD_K, 0.0f);
  Vec3 wo = Vec3(0.5f, 0.0f, sqrtf(0.75f));
  ShadingData sd = makeTestSD(wo);
  Vec3 woLocal     = sd.frame.toLocal(wo);
  Vec3 mirrorWorld = sd.frame.toWorld(ShadingHelper::reflect(woLocal));
  MTRandom sampler;
  for (int i = 0; i < 20; i++) {
    Colour col; float pdf;
    Vec3 wi = bsdf.sample(sd, &sampler, col, pdf);
    if (pdf > 0.0f) {
      float dotVal = wi.x * mirrorWorld.x + wi.y * mirrorWorld.y + wi.z * mirrorWorld.z;
      REQUIRE(dotVal > 0.99f); // within ~8° of mirror direction for alpha=0.01
    }
  }
}
