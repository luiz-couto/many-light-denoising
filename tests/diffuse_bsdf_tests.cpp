#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "shading.h"
#include "texture.h"
#include "sampling.h"
#include "bsdf_test_utils.h"
#include <cmath>

// Helpers — build a white 1x1 DiffuseBSDF and a fixed shading point on the flat XY plane.
static Texture makeWhiteTex() {
  Texture t;
  t.loadDefault(); // 1x1 white
  return t;
}

static ShadingData makeFlatShadingData(Texture* tex) {
  Vec3 x(0.0f, 0.0f, 0.0f);
  Vec3 n(0.0f, 0.0f, 1.0f);
  ShadingData sd(x, n);
  sd.tu = 0.0f;
  sd.tv = 0.0f;
  return sd;
}

// -----------------------------------------------------------------------
// evaluate — Lambertian BRDF = albedo / π
// -----------------------------------------------------------------------

TEST_CASE("DiffuseBSDF evaluate: white albedo returns 1/pi") {
  Texture tex = makeWhiteTex();
  DiffuseBSDF bsdf(&tex);
  ShadingData sd = makeFlatShadingData(&tex);
  Vec3 wi(0.0f, 0.0f, 1.0f);
  Colour result = bsdf.evaluate(sd, wi);
  float expected = 1.0f / M_PI;
  REQUIRE(result.r == Catch::Approx(expected).margin(0.0001f));
  REQUIRE(result.g == Catch::Approx(expected).margin(0.0001f));
  REQUIRE(result.b == Catch::Approx(expected).margin(0.0001f));
}

TEST_CASE("DiffuseBSDF evaluate: result is direction-independent (Lambertian)") {
  Texture tex = makeWhiteTex();
  DiffuseBSDF bsdf(&tex);
  ShadingData sd = makeFlatShadingData(&tex);
  Colour a = bsdf.evaluate(sd, Vec3(0.0f, 0.0f, 1.0f));
  Colour b = bsdf.evaluate(sd, Vec3(0.577f, 0.577f, 0.577f));
  REQUIRE(a.r == Catch::Approx(b.r).margin(0.0001f));
}

TEST_CASE("DiffuseBSDF evaluate: scales with albedo") {
  // Grey albedo (0.5, 0.5, 0.5) → BRDF = 0.5/π
  Texture tex;
  tex.width = tex.height = tex.channels = 1;
  tex.texels = new Colour[1];
  tex.texels[0] = Colour(0.5f, 0.5f, 0.5f);

  DiffuseBSDF bsdf(&tex);
  ShadingData sd = makeFlatShadingData(&tex);
  Colour result = bsdf.evaluate(sd, Vec3(0.0f, 0.0f, 1.0f));
  REQUIRE(result.r == Catch::Approx(0.5f / M_PI).margin(0.0001f));
}

// -----------------------------------------------------------------------
// PDF — cosine-weighted hemisphere: p(ω) = cosθ / π
// -----------------------------------------------------------------------

TEST_CASE("DiffuseBSDF PDF: normal incidence wi=(0,0,1) gives 1/pi") {
  Texture tex = makeWhiteTex();
  DiffuseBSDF bsdf(&tex);
  ShadingData sd = makeFlatShadingData(&tex);
  // wi along normal → cosθ=1 → pdf = 1/π
  Vec3 wi(0.0f, 0.0f, 1.0f);
  REQUIRE(bsdf.PDF(sd, wi) == Catch::Approx(1.0f / M_PI).margin(0.0001f));
}

TEST_CASE("DiffuseBSDF PDF: 45 degree direction gives cos(45)/pi") {
  Texture tex = makeWhiteTex();
  DiffuseBSDF bsdf(&tex);
  ShadingData sd = makeFlatShadingData(&tex);
  float c45 = sqrtf(0.5f);
  Vec3 wi(c45, 0.0f, c45); // 45° from normal
  REQUIRE(bsdf.PDF(sd, wi) == Catch::Approx(c45 / M_PI).margin(0.001f));
}

TEST_CASE("DiffuseBSDF PDF: below hemisphere gives 0") {
  Texture tex = makeWhiteTex();
  DiffuseBSDF bsdf(&tex);
  ShadingData sd = makeFlatShadingData(&tex);
  Vec3 wi(0.0f, 0.0f, -1.0f); // pointing away from surface
  REQUIRE(bsdf.PDF(sd, wi) == Catch::Approx(0.0f));
}

// -----------------------------------------------------------------------
// sample — weight correctness: reflectedColour = albedo (f*cosθ/pdf cancels)
// -----------------------------------------------------------------------

TEST_CASE("DiffuseBSDF sample: weight equals albedo for white texture") {
  // f*cosθ/pdf = (albedo/π) * cosθ / (cosθ/π) = albedo
  Texture tex = makeWhiteTex();
  DiffuseBSDF bsdf(&tex);
  ShadingData sd = makeFlatShadingData(&tex);
  MTRandom sampler;
  Colour weight;
  float pdf;
  bsdf.sample(sd, &sampler, weight, pdf);
  REQUIRE(weight.r == Catch::Approx(1.0f).margin(0.0001f));
  REQUIRE(weight.g == Catch::Approx(1.0f).margin(0.0001f));
  REQUIRE(weight.b == Catch::Approx(1.0f).margin(0.0001f));
}

TEST_CASE("DiffuseBSDF sample: weight equals albedo for grey texture") {
  Texture tex;
  tex.width = tex.height = tex.channels = 1;
  tex.texels = new Colour[1];
  tex.texels[0] = Colour(0.5f, 0.5f, 0.5f);

  DiffuseBSDF bsdf(&tex);
  ShadingData sd = makeFlatShadingData(&tex);
  MTRandom sampler;
  Colour weight;
  float pdf;
  bsdf.sample(sd, &sampler, weight, pdf);
  REQUIRE(weight.r == Catch::Approx(0.5f).margin(0.0001f));
}

TEST_CASE("DiffuseBSDF sample: weight equals evaluate * cosTheta / pdf") {
  // Consistency: weight returned by sample must equal f(wi)*cosθ/pdf(wi)
  Texture tex = makeWhiteTex();
  DiffuseBSDF bsdf(&tex);
  ShadingData sd = makeFlatShadingData(&tex);
  MTRandom sampler;
  Colour weight;
  float pdf;
  Vec3 wi = bsdf.sample(sd, &sampler, weight, pdf);

  Vec3 localWi = sd.frame.toLocal(wi);
  float cosTheta = std::max(0.0f, localWi.z);
  Colour brdf = bsdf.evaluate(sd, wi);
  Colour expected = brdf * cosTheta / pdf;

  REQUIRE(weight.r == Catch::Approx(expected.r).margin(0.001f));
  REQUIRE(weight.g == Catch::Approx(expected.g).margin(0.001f));
  REQUIRE(weight.b == Catch::Approx(expected.b).margin(0.001f));
}

TEST_CASE("DiffuseBSDF sample: returned pdf matches PDF()") {
  Texture tex = makeWhiteTex();
  DiffuseBSDF bsdf(&tex);
  ShadingData sd = makeFlatShadingData(&tex);
  MTRandom sampler;
  Colour weight;
  float pdf;
  Vec3 wi = bsdf.sample(sd, &sampler, weight, pdf);
  REQUIRE(pdf == Catch::Approx(bsdf.PDF(sd, wi)).margin(0.0001f));
}

TEST_CASE("DiffuseBSDF sample: returned direction is in upper hemisphere") {
  Texture tex = makeWhiteTex();
  DiffuseBSDF bsdf(&tex);
  ShadingData sd = makeFlatShadingData(&tex);
  MTRandom sampler;
  for (int i = 0; i < 100; i++) {
    Colour weight; float pdf;
    Vec3 wi = bsdf.sample(sd, &sampler, weight, pdf);
    Vec3 localWi = sd.frame.toLocal(wi);
    REQUIRE(localWi.z >= -0.001f); // must be on or above hemisphere
  }
}

// -----------------------------------------------------------------------
// flags
// -----------------------------------------------------------------------

TEST_CASE("DiffuseBSDF isPureSpecular: returns false") {
  Texture tex = makeWhiteTex();
  DiffuseBSDF bsdf(&tex);
  REQUIRE(bsdf.isPureSpecular() == false);
}

TEST_CASE("DiffuseBSDF isTwoSided: returns true") {
  Texture tex = makeWhiteTex();
  DiffuseBSDF bsdf(&tex);
  REQUIRE(bsdf.isTwoSided() == true);
}

// -----------------------------------------------------------------------
// Energy conservation
// -----------------------------------------------------------------------

TEST_CASE("DiffuseBSDF energy conservation: reflectance <= 1 for white albedo") {
  Texture tex = makeWhiteTex();
  DiffuseBSDF bsdf(&tex);
  MTRandom sampler;
  // Test a range of outgoing directions from grazing to near-normal
  float cosines[] = { 0.1f, 0.3f, 0.5f, 0.7f, 0.9f, 1.0f };
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

TEST_CASE("DiffuseBSDF chi-square: sampled distribution matches PDF") {
  Texture tex = makeWhiteTex();
  DiffuseBSDF bsdf(&tex);
  // 45° outgoing direction exercises the full cosine-weighted lobe
  ShadingData sd = makeTestSD(Vec3(sqrtf(0.5f), 0.0f, sqrtf(0.5f)));
  REQUIRE(chiSquareTest(&bsdf, sd));
}
