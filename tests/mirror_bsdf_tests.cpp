#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "shading.h"
#include "texture.h"
#include "sampling.h"
#include "bsdf_test_utils.h"
#include <cmath>

static Texture makeWhiteTex() { Texture t; t.loadDefault(); return t; }

// Build a flat shading point with normal (0,0,1) and a given outgoing direction.
static ShadingData makeSD(Texture* tex, Vec3 wo) {
  ShadingData sd(Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f));
  sd.wo = wo;
  sd.tu = sd.tv = 0.0f;
  return sd;
}

// -----------------------------------------------------------------------
// Reflection direction — geometric correctness
// -----------------------------------------------------------------------

TEST_CASE("MirrorBSDF sample: normal incidence wi points along normal") {
  // wo straight up → perfect mirror reflects straight back along normal
  Texture tex = makeWhiteTex();
  MirrorBSDF bsdf(&tex, Colour(1.0f, 1.0f, 1.0f), Colour(100.0f, 100.0f, 100.0f));
  ShadingData sd = makeSD(&tex, Vec3(0.0f, 0.0f, 1.0f));
  MTRandom sampler;
  Colour weight; float pdf;
  Vec3 wi = bsdf.sample(sd, &sampler, weight, pdf);
  Vec3 localWi = sd.frame.toLocal(wi);
  REQUIRE(localWi.z == Catch::Approx(1.0f).margin(0.001f));
  REQUIRE(fabsf(localWi.x) < 0.001f);
  REQUIRE(fabsf(localWi.y) < 0.001f);
}

TEST_CASE("MirrorBSDF sample: reflection preserves angle with normal (cosTheta)") {
  // dot(wi, n) must equal dot(wo, n) — law of reflection
  float c = 0.7f;
  Texture tex = makeWhiteTex();
  MirrorBSDF bsdf(&tex, Colour(1.0f, 1.0f, 1.0f), Colour(100.0f, 100.0f, 100.0f));
  ShadingData sd = makeSD(&tex, Vec3(sqrtf(1.0f - c * c), 0.0f, c));
  MTRandom sampler;
  Colour weight; float pdf;
  Vec3 wi = bsdf.sample(sd, &sampler, weight, pdf);
  Vec3 localWi = sd.frame.toLocal(wi);
  Vec3 localWo = sd.frame.toLocal(sd.wo);
  REQUIRE(fabsf(localWi.z) == Catch::Approx(fabsf(localWo.z)).margin(0.001f));
}

TEST_CASE("MirrorBSDF sample: tangential component magnitude preserved") {
  // |wi_tangent| == |wo_tangent| — reflection only flips the tangent sign
  float c = 0.6f;
  Texture tex = makeWhiteTex();
  MirrorBSDF bsdf(&tex, Colour(1.0f, 1.0f, 1.0f), Colour(100.0f, 100.0f, 100.0f));
  ShadingData sd = makeSD(&tex, Vec3(sqrtf(1.0f - c * c), 0.0f, c));
  MTRandom sampler;
  Colour weight; float pdf;
  Vec3 wi = bsdf.sample(sd, &sampler, weight, pdf);
  Vec3 localWi = sd.frame.toLocal(wi);
  Vec3 localWo = sd.frame.toLocal(sd.wo);
  float tangWi = sqrtf(localWi.x * localWi.x + localWi.y * localWi.y);
  float tangWo = sqrtf(localWo.x * localWo.x + localWo.y * localWo.y);
  REQUIRE(tangWi == Catch::Approx(tangWo).margin(0.001f));
}

TEST_CASE("MirrorBSDF sample: wi stays in upper hemisphere") {
  Texture tex = makeWhiteTex();
  MirrorBSDF bsdf(&tex, Colour(1.0f, 1.0f, 1.0f), Colour(100.0f, 100.0f, 100.0f));
  // Multiple wo directions, all in upper hemisphere
  float angles[] = {0.99f, 0.7f, 0.4f, 0.1f};
  for (float c : angles) {
    ShadingData sd = makeSD(&tex, Vec3(sqrtf(1.0f - c * c), 0.0f, c));
    MTRandom sampler;
    Colour weight; float pdf;
    Vec3 wi = bsdf.sample(sd, &sampler, weight, pdf);
    Vec3 localWi = sd.frame.toLocal(wi);
    REQUIRE(localWi.z > 0.0f);
  }
}

// -----------------------------------------------------------------------
// pdf
// -----------------------------------------------------------------------

TEST_CASE("MirrorBSDF sample: pdf is always 1") {
  Texture tex = makeWhiteTex();
  MirrorBSDF bsdf(&tex, Colour(1.0f, 1.0f, 1.0f), Colour(100.0f, 100.0f, 100.0f));
  ShadingData sd = makeSD(&tex, Vec3(0.0f, 0.0f, 1.0f));
  MTRandom sampler;
  Colour weight; float pdf;
  bsdf.sample(sd, &sampler, weight, pdf);
  REQUIRE(pdf == Catch::Approx(1.0f));
}

TEST_CASE("MirrorBSDF PDF: returns 0 (delta BSDF, not sampled by MIS)") {
  Texture tex = makeWhiteTex();
  MirrorBSDF bsdf(&tex, Colour(1.0f, 1.0f, 1.0f), Colour(1.0f, 1.0f, 1.0f));
  ShadingData sd = makeSD(&tex, Vec3(0.0f, 0.0f, 1.0f));
  REQUIRE(bsdf.PDF(sd, Vec3(0.0f, 0.0f, 1.0f)) == Catch::Approx(0.0f));
}

// -----------------------------------------------------------------------
// Weight correctness — sample's reflectedColour = F * albedo
// -----------------------------------------------------------------------

TEST_CASE("MirrorBSDF sample: perfect mirror weight is white for white albedo") {
  // k→∞ → R→1 for all angles, so weight = R * albedo = white
  Texture tex = makeWhiteTex();
  MirrorBSDF bsdf(&tex, Colour(1.0f, 1.0f, 1.0f), Colour(1000.0f, 1000.0f, 1000.0f));
  ShadingData sd = makeSD(&tex, Vec3(0.0f, 0.0f, 1.0f));
  MTRandom sampler;
  Colour weight; float pdf;
  bsdf.sample(sd, &sampler, weight, pdf);
  REQUIRE(weight.r == Catch::Approx(1.0f).margin(0.001f));
  REQUIRE(weight.g == Catch::Approx(1.0f).margin(0.001f));
  REQUIRE(weight.b == Catch::Approx(1.0f).margin(0.001f));
}

TEST_CASE("MirrorBSDF sample: weight matches fresnelConductor * albedo") {
  Texture tex = makeWhiteTex();
  Colour eta(0.37f, 0.37f, 0.37f), k(2.82f, 2.82f, 2.82f);
  MirrorBSDF bsdf(&tex, eta, k);
  ShadingData sd = makeSD(&tex, Vec3(0.0f, 0.0f, 1.0f)); // normal incidence, cosTheta=1
  MTRandom sampler;
  Colour weight; float pdf;
  bsdf.sample(sd, &sampler, weight, pdf);
  Colour expected = ShadingHelper::fresnelConductor(1.0f, eta, k); // albedo=white so * 1
  REQUIRE(weight.r == Catch::Approx(expected.r).margin(0.001f));
  REQUIRE(weight.g == Catch::Approx(expected.g).margin(0.001f));
  REQUIRE(weight.b == Catch::Approx(expected.b).margin(0.001f));
}

TEST_CASE("MirrorBSDF sample: weight = evaluate * cosTheta (delta BSDF identity)") {
  // weight = f(wi) * cosθ / pdf = evaluate(wi) * cosθ * 1
  Texture tex = makeWhiteTex();
  MirrorBSDF bsdf(&tex, Colour(0.37f, 0.37f, 0.37f), Colour(2.82f, 2.82f, 2.82f));
  ShadingData sd = makeSD(&tex, Vec3(sqrtf(0.5f), 0.0f, sqrtf(0.5f)));
  MTRandom sampler;
  Colour weight; float pdf;
  Vec3 wi = bsdf.sample(sd, &sampler, weight, pdf);
  Vec3 localWi = sd.frame.toLocal(wi);
  float cosTheta = fabsf(localWi.z);
  Colour eval = bsdf.evaluate(sd, wi);
  REQUIRE(weight.r == Catch::Approx(eval.r * cosTheta).margin(0.001f));
  REQUIRE(weight.g == Catch::Approx(eval.g * cosTheta).margin(0.001f));
  REQUIRE(weight.b == Catch::Approx(eval.b * cosTheta).margin(0.001f));
}

// -----------------------------------------------------------------------
// evaluate — f(wi) = F(cosθ) * albedo / cosθ
// -----------------------------------------------------------------------

TEST_CASE("MirrorBSDF evaluate: k=0, n=1.5, normal incidence gives 0.04 (dielectric limit)") {
  // fresnelConductor(1, {1.5,1.5,1.5}, {0,0,0}) = ((1.5-1)/(1.5+1))^2 = 0.04
  // evaluate = 0.04 * white / 1.0 = 0.04
  Texture tex = makeWhiteTex();
  MirrorBSDF bsdf(&tex, Colour(1.5f, 1.5f, 1.5f), Colour(0.0f, 0.0f, 0.0f));
  ShadingData sd = makeSD(&tex, Vec3(0.0f, 0.0f, 1.0f));
  Colour result = bsdf.evaluate(sd, Vec3(0.0f, 0.0f, 1.0f));
  REQUIRE(result.r == Catch::Approx(0.04f).margin(0.002f));
  REQUIRE(result.g == Catch::Approx(0.04f).margin(0.002f));
  REQUIRE(result.b == Catch::Approx(0.04f).margin(0.002f));
}

TEST_CASE("MirrorBSDF evaluate: result is non-negative for all directions") {
  Texture tex = makeWhiteTex();
  MirrorBSDF bsdf(&tex, Colour(0.37f, 0.37f, 0.37f), Colour(2.82f, 2.82f, 2.82f));
  ShadingData sd = makeSD(&tex, Vec3(0.0f, 0.0f, 1.0f));
  for (int i = 1; i <= 9; i++) {
    float c = i / 10.0f;
    Vec3 wi(sqrtf(1.0f - c * c), 0.0f, c);
    Colour result = bsdf.evaluate(sd, wi);
    REQUIRE(result.r >= 0.0f);
    REQUIRE(result.g >= 0.0f);
    REQUIRE(result.b >= 0.0f);
  }
}

TEST_CASE("MirrorBSDF evaluate: scales linearly with albedo") {
  // evaluate is proportional to albedo — halving albedo halves the result
  Texture whiteTex = makeWhiteTex();

  Texture greyTex;
  greyTex.width = greyTex.height = greyTex.channels = 1;
  greyTex.texels = new Colour[1];
  greyTex.texels[0] = Colour(0.5f, 0.5f, 0.5f);

  Colour eta(0.37f, 0.37f, 0.37f), k(2.82f, 2.82f, 2.82f);
  MirrorBSDF white_bsdf(&whiteTex, eta, k);
  MirrorBSDF grey_bsdf(&greyTex, eta, k);

  ShadingData sd = makeSD(&whiteTex, Vec3(0.0f, 0.0f, 1.0f));
  Vec3 wi(0.0f, 0.0f, 1.0f);
  Colour r_white = white_bsdf.evaluate(sd, wi);
  Colour r_grey  = grey_bsdf.evaluate(sd, wi);
  REQUIRE(r_grey.r == Catch::Approx(r_white.r * 0.5f).margin(0.001f));
}

// -----------------------------------------------------------------------
// Edge cases
// -----------------------------------------------------------------------

TEST_CASE("MirrorBSDF sample: near-grazing incidence weight approaches 1") {
  // All conductors approach R=1 at grazing — weight should be close to 1
  Texture tex = makeWhiteTex();
  MirrorBSDF bsdf(&tex, Colour(0.37f, 0.37f, 0.37f), Colour(2.82f, 2.82f, 2.82f));
  float c = 0.01f; // nearly 90° from normal
  ShadingData sd = makeSD(&tex, Vec3(sqrtf(1.0f - c * c), 0.0f, c));
  MTRandom sampler;
  Colour weight; float pdf;
  bsdf.sample(sd, &sampler, weight, pdf);
  REQUIRE(weight.r > 0.99f);
  REQUIRE(weight.g > 0.99f);
  REQUIRE(weight.b > 0.99f);
}

TEST_CASE("MirrorBSDF sample: weight in [0, 1] for all angles") {
  Texture tex = makeWhiteTex();
  MirrorBSDF bsdf(&tex, Colour(0.37f, 0.37f, 0.37f), Colour(2.82f, 2.82f, 2.82f));
  float angles[] = {0.99f, 0.8f, 0.6f, 0.4f, 0.2f, 0.05f};
  for (float c : angles) {
    ShadingData sd = makeSD(&tex, Vec3(sqrtf(1.0f - c * c), 0.0f, c));
    MTRandom sampler;
    Colour weight; float pdf;
    bsdf.sample(sd, &sampler, weight, pdf);
    REQUIRE(weight.r >= 0.0f); REQUIRE(weight.r <= 1.0f);
    REQUIRE(weight.g >= 0.0f); REQUIRE(weight.g <= 1.0f);
    REQUIRE(weight.b >= 0.0f); REQUIRE(weight.b <= 1.0f);
  }
}

// -----------------------------------------------------------------------
// Flags
// -----------------------------------------------------------------------

TEST_CASE("MirrorBSDF isPureSpecular: returns true") {
  Texture tex = makeWhiteTex();
  MirrorBSDF bsdf(&tex, Colour(1.0f, 1.0f, 1.0f), Colour(1.0f, 1.0f, 1.0f));
  REQUIRE(bsdf.isPureSpecular() == true);
}

// -----------------------------------------------------------------------
// Energy conservation
// -----------------------------------------------------------------------

TEST_CASE("MirrorBSDF energy conservation: reflectance <= 1") {
  // Silver IOR — Fresnel < 1 at all angles so reflectance < 1
  Texture tex = makeWhiteTex();
  MirrorBSDF bsdf(&tex, Colour(0.177f, 0.178f, 0.172f), Colour(3.638f, 2.973f, 2.430f));
  float cosines[] = { 0.2f, 0.4f, 0.6f, 0.8f, 1.0f };
  for (float c : cosines) {
    float s = sqrtf(1.0f - c * c);
    ShadingData sd = makeTestSD(Vec3(s, 0.0f, c));
    float r = estimateReflectance(&bsdf, sd, 500); // deterministic — 500 samples is plenty
    REQUIRE(r <= 1.05f);
  }
}
