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

// Standard air / glass pair
static constexpr float AIR   = 1.0f;
static constexpr float GLASS = 1.5f;

// -----------------------------------------------------------------------
// ShadingHelper::reflect
// -----------------------------------------------------------------------

TEST_CASE("ShadingHelper reflect: flips tangential components, preserves normal component") {
  Vec3 wo(0.6f, 0.0f, 0.8f);
  Vec3 wr = ShadingHelper::reflect(wo);
  REQUIRE(wr.x == Catch::Approx(-wo.x));
  REQUIRE(wr.y == Catch::Approx(-wo.y));
  REQUIRE(wr.z == Catch::Approx( wo.z));
}

// -----------------------------------------------------------------------
// ShadingHelper::refract — Snell's law direction
// -----------------------------------------------------------------------

TEST_CASE("ShadingHelper refract: normal incidence passes straight through") {
  // wo=(0,0,1), n=1.5 (air→glass): no bending at normal incidence
  bool tir;
  Vec3 wt = ShadingHelper::refract(Vec3(0.0f, 0.0f, 1.0f), GLASS / AIR, tir);
  REQUIRE_FALSE(tir);
  REQUIRE(fabsf(wt.x) < 0.001f);
  REQUIRE(fabsf(wt.y) < 0.001f);
  REQUIRE(wt.z == Catch::Approx(-1.0f).margin(0.001f)); // transmitted, going down
}

TEST_CASE("ShadingHelper refract: satisfies Snell's law sin(theta_i)/sin(theta_t) = n_t/n_i") {
  // wo = (sin30°, 0, cos30°), air→glass
  float sinI = 0.5f, cosI = sqrtf(0.75f);
  bool tir;
  Vec3 wt = ShadingHelper::refract(Vec3(sinI, 0.0f, cosI), GLASS / AIR, tir);
  REQUIRE_FALSE(tir);
  float sinT = sqrtf(wt.x * wt.x + wt.y * wt.y); // tangential magnitude = sinT
  REQUIRE(sinT == Catch::Approx(sinI * AIR / GLASS).margin(0.001f));
}

TEST_CASE("ShadingHelper refract: returned direction is unit length") {
  bool tir;
  Vec3 wt = ShadingHelper::refract(Vec3(0.5f, 0.0f, sqrtf(0.75f)), GLASS / AIR, tir);
  float len = sqrtf(wt.x * wt.x + wt.y * wt.y + wt.z * wt.z);
  REQUIRE(len == Catch::Approx(1.0f).margin(0.001f));
}

TEST_CASE("ShadingHelper refract: TIR at steep angle glass→air") {
  // Critical angle: sin_c = n_air/n_glass = 1/1.5 ≈ 0.667 → theta_c ≈ 41.8°
  // At 60° (sin=0.866 > 0.667) TIR must occur
  float sin60 = sqrtf(0.75f), cos60 = 0.5f;
  bool tir;
  // wo.z < 0: exiting glass; n = n_t/n_i = extIOR/intIOR = air/glass
  Vec3 wt = ShadingHelper::refract(Vec3(sin60, 0.0f, -cos60), AIR / GLASS, tir);
  REQUIRE(tir);
  // reflect preserves z: wr.z = wo.z = -cos60 (ray bounces back into glass, staying below horizon)
  REQUIRE(wt.z == Catch::Approx(-cos60).margin(0.001f));
}

TEST_CASE("ShadingHelper refract: no TIR air→glass at any angle") {
  // Going from sparse to dense medium — TIR never occurs
  for (float sinI : {0.1f, 0.3f, 0.5f, 0.7f, 0.9f, 0.99f}) {
    float cosI = sqrtf(1.0f - sinI * sinI);
    bool tir;
    ShadingHelper::refract(Vec3(sinI, 0.0f, cosI), GLASS / AIR, tir);
    REQUIRE_FALSE(tir);
  }
}

TEST_CASE("ShadingHelper refract: exiting direction goes into upper hemisphere") {
  // wo.z < 0 (exiting glass, below-critical angle): transmitted ray should go upward
  float sinI = 0.3f, cosI = sqrtf(1.0f - sinI * sinI);
  bool tir;
  Vec3 wt = ShadingHelper::refract(Vec3(sinI, 0.0f, -cosI), AIR / GLASS, tir);
  REQUIRE_FALSE(tir);
  REQUIRE(wt.z > 0.0f); // exits upward into air
}

// -----------------------------------------------------------------------
// GlassBSDF::evaluate — always zero (delta BSDF)
// -----------------------------------------------------------------------

TEST_CASE("GlassBSDF evaluate: returns zero for any wi/wo pair") {
  Texture tex = makeWhiteTex();
  GlassBSDF bsdf(&tex, GLASS, AIR);
  ShadingData sd = makeSD(Vec3(0.0f, 0.0f, 1.0f));
  REQUIRE(bsdf.evaluate(sd, Vec3(0.0f, 0.0f, 1.0f)).r == Catch::Approx(0.0f));
  REQUIRE(bsdf.evaluate(sd, Vec3(0.5f, 0.0f, sqrtf(0.75f))).r == Catch::Approx(0.0f));
}

// -----------------------------------------------------------------------
// GlassBSDF::PDF — always zero
// -----------------------------------------------------------------------

TEST_CASE("GlassBSDF PDF: returns zero") {
  Texture tex = makeWhiteTex();
  GlassBSDF bsdf(&tex, GLASS, AIR);
  ShadingData sd = makeSD(Vec3(0.0f, 0.0f, 1.0f));
  REQUIRE(bsdf.PDF(sd, Vec3(0.0f, 0.0f, 1.0f)) == Catch::Approx(0.0f));
}

// -----------------------------------------------------------------------
// GlassBSDF::isPureSpecular and isTwoSided
// -----------------------------------------------------------------------

TEST_CASE("GlassBSDF isPureSpecular: returns true") {
  Texture tex = makeWhiteTex();
  GlassBSDF bsdf(&tex, GLASS, AIR);
  REQUIRE(bsdf.isPureSpecular() == true);
}

TEST_CASE("GlassBSDF isTwoSided: returns false (one-sided glass)") {
  Texture tex = makeWhiteTex();
  GlassBSDF bsdf(&tex, GLASS, AIR);
  REQUIRE(bsdf.isTwoSided() == false);
}

// -----------------------------------------------------------------------
// GlassBSDF::sample — TIR forces reflection
// -----------------------------------------------------------------------

TEST_CASE("GlassBSDF sample: TIR at steep exiting angle always reflects") {
  // Glass→air at 60° is past the critical angle — every sample must reflect
  Texture tex = makeWhiteTex();
  GlassBSDF bsdf(&tex, GLASS, AIR);
  float sin60 = sqrtf(0.75f), cos60 = 0.5f;
  ShadingData sd = makeSD(Vec3(sin60, 0.0f, -cos60)); // wo.z < 0: exiting
  MTRandom sampler;

  for (int i = 0; i < 50; i++) {
    Colour weight; float pdf;
    Vec3 wi = bsdf.sample(sd, &sampler, weight, pdf);
    Vec3 localWi = sd.frame.toLocal(wi);
    // TIR: pdf=1, wi must be the reflection (same-sign z as wo.z)
    REQUIRE(pdf == Catch::Approx(1.0f).margin(0.001f));
    REQUIRE(localWi.z == Catch::Approx(-cos60).margin(0.01f)); // reflected, below horizon
  }
}

// -----------------------------------------------------------------------
// GlassBSDF::sample — reflectedColour equals albedo
// -----------------------------------------------------------------------

TEST_CASE("GlassBSDF sample: reflectedColour equals albedo") {
  // The Fresnel probability cancels the BSDF weight, leaving just albedo
  Texture tex = makeWhiteTex();
  GlassBSDF bsdf(&tex, GLASS, AIR);
  ShadingData sd = makeSD(Vec3(0.0f, 0.0f, 1.0f));
  MTRandom sampler;

  for (int i = 0; i < 30; i++) {
    Colour weight; float pdf;
    bsdf.sample(sd, &sampler, weight, pdf);
    REQUIRE(weight.r == Catch::Approx(1.0f).margin(0.001f));
    REQUIRE(weight.g == Catch::Approx(1.0f).margin(0.001f));
    REQUIRE(weight.b == Catch::Approx(1.0f).margin(0.001f));
  }
}

// -----------------------------------------------------------------------
// GlassBSDF::sample — pdf matches Fresnel probability
// -----------------------------------------------------------------------

TEST_CASE("GlassBSDF sample: pdf is Fresnel when reflecting, 1-Fresnel when refracting") {
  Texture tex = makeWhiteTex();
  GlassBSDF bsdf(&tex, GLASS, AIR);
  Vec3 wo(0.0f, 0.0f, 1.0f);
  ShadingData sd = makeSD(wo);
  MTRandom sampler;

  float expectedFresnel = ShadingHelper::fresnelDielectric(1.0f, GLASS / AIR);

  for (int i = 0; i < 100; i++) {
    Colour weight; float pdf;
    Vec3 wi = bsdf.sample(sd, &sampler, weight, pdf);
    Vec3 localWi = sd.frame.toLocal(wi);

    if (localWi.z > 0.0f) { // reflected (same hemisphere as wo)
      REQUIRE(pdf == Catch::Approx(expectedFresnel).margin(0.001f));
    } else { // refracted
      REQUIRE(pdf == Catch::Approx(1.0f - expectedFresnel).margin(0.001f));
    }
  }
}

// -----------------------------------------------------------------------
// GlassBSDF::sample — reflection direction satisfies law of reflection
// -----------------------------------------------------------------------

TEST_CASE("GlassBSDF sample: reflected wi has correct angle with normal") {
  // Force a case that always reflects: steep glass→air exiting (TIR)
  Texture tex = makeWhiteTex();
  GlassBSDF bsdf(&tex, GLASS, AIR);
  float c = 0.5f;
  ShadingData sd = makeSD(Vec3(sqrtf(1.0f - c * c), 0.0f, -c)); // exiting, TIR
  MTRandom sampler;

  Colour weight; float pdf;
  Vec3 wi = bsdf.sample(sd, &sampler, weight, pdf);
  Vec3 localWi = sd.frame.toLocal(wi);
  Vec3 localWo = sd.frame.toLocal(sd.wo);
  // Law of reflection: |wi.z| = |wo.z|
  REQUIRE(fabsf(localWi.z) == Catch::Approx(fabsf(localWo.z)).margin(0.001f));
  // Tangential components negate
  REQUIRE(localWi.x == Catch::Approx(-localWo.x).margin(0.001f));
}

// -----------------------------------------------------------------------
// GlassBSDF::sample — refracted direction satisfies Snell's law
// -----------------------------------------------------------------------

TEST_CASE("GlassBSDF sample: refracted wi satisfies Snell's law at normal incidence") {
  // At normal incidence the refracted ray goes straight through (no bending)
  // n_air=1, n_glass=1.5: Fresnel ≈ 0.04 so ~96% of samples refract
  // Run enough iterations to confirm at least one refracted sample
  Texture tex = makeWhiteTex();
  GlassBSDF bsdf(&tex, GLASS, AIR);
  ShadingData sd = makeSD(Vec3(0.0f, 0.0f, 1.0f));
  MTRandom sampler;

  bool foundRefracted = false;
  for (int i = 0; i < 200; i++) {
    Colour weight; float pdf;
    Vec3 wi = bsdf.sample(sd, &sampler, weight, pdf);
    Vec3 localWi = sd.frame.toLocal(wi);
    if (localWi.z < 0.0f) { // refracted
      // At normal incidence, tangential components stay zero
      REQUIRE(fabsf(localWi.x) < 0.01f);
      REQUIRE(fabsf(localWi.y) < 0.01f);
      REQUIRE(localWi.z == Catch::Approx(-1.0f).margin(0.01f));
      foundRefracted = true;
      break;
    }
  }
  REQUIRE(foundRefracted);
}

TEST_CASE("GlassBSDF sample: refracted wi satisfies Snell's law at oblique incidence") {
  // wo = (sin30°, 0, cos30°), air→glass; sinT = sinI * n_air/n_glass = 0.5/1.5 = 0.333
  Texture tex = makeWhiteTex();
  GlassBSDF bsdf(&tex, GLASS, AIR);
  ShadingData sd = makeSD(Vec3(0.5f, 0.0f, sqrtf(0.75f)));
  MTRandom sampler;

  float expectedSinT = 0.5f * AIR / GLASS;

  for (int attempt = 0; attempt < 500; attempt++) {
    Colour weight; float pdf;
    Vec3 wi = bsdf.sample(sd, &sampler, weight, pdf);
    Vec3 localWi = sd.frame.toLocal(wi);
    if (localWi.z < 0.0f) {
      float sinT = sqrtf(localWi.x * localWi.x + localWi.y * localWi.y);
      REQUIRE(sinT == Catch::Approx(expectedSinT).margin(0.005f));
      return;
    }
  }
  FAIL("No refracted sample produced in 500 attempts");
}

// -----------------------------------------------------------------------
// GlassBSDF::sample — no TIR for air→glass at any angle (n convention fix)
//
// With the old buggy n (extIOR/intIOR for entering = n_i/n_t),
// getCosThetaT got term < 0 at large air→glass angles and wrongly reflected.
// With the fix (n = n_t/n_i), no TIR ever occurs air→glass.
// -----------------------------------------------------------------------

TEST_CASE("GlassBSDF sample: air→glass never TIRs, some samples always refract") {
  Texture tex = makeWhiteTex();
  GlassBSDF bsdf(&tex, GLASS, AIR);

  // Test at a steep angle where the old code triggered false TIR (sin≈0.8, theta≈53°)
  float sinI = 0.8f, cosI = sqrtf(1.0f - sinI * sinI);
  ShadingData sd = makeSD(Vec3(sinI, 0.0f, cosI));
  MTRandom sampler;

  int refractCount = 0;
  for (int i = 0; i < 200; i++) {
    Colour weight; float pdf;
    Vec3 wi = bsdf.sample(sd, &sampler, weight, pdf);
    Vec3 localWi = sd.frame.toLocal(wi);
    if (localWi.z < 0.0f) refractCount++;
  }
  // With the bug all samples were TIR-reflected; with the fix most should refract
  REQUIRE(refractCount > 100);
}

// -----------------------------------------------------------------------
// Energy conservation
// -----------------------------------------------------------------------

TEST_CASE("GlassBSDF energy conservation: white albedo is lossless (reflectance == 1)") {
  // Glass is a pure dielectric — every photon is either reflected or refracted
  // without absorption. With white albedo, reflectedColour = albedo = 1 in
  // both branches, so the Monte Carlo estimate should be exactly 1.0.
  Texture tex = makeWhiteTex();
  GlassBSDF bsdf(&tex, GLASS, AIR);
  float cosines[] = { 0.2f, 0.5f, 0.8f, 1.0f };
  for (float c : cosines) {
    float s = sqrtf(1.0f - c * c);
    ShadingData sd = makeTestSD(Vec3(s, 0.0f, c));
    float r = estimateReflectance(&bsdf, sd, 500);
    REQUIRE(r == Catch::Approx(1.0f).margin(0.05f));
  }
}
