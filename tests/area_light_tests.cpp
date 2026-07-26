#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "light.h"
#include "geometry.h"
#include "sampling.h"
#include <cmath>

// Flat triangle in the XZ plane with normal pointing up (+Y).
// v0=(0,0,0), v1=(1,0,0), v2=(0,0,1) → area = 0.5, gNormal = (0,1,0)
static Triangle makeFloorTriangle() {
  Triangle t;
  Vec3 up(0.0f, 1.0f, 0.0f);
  Vertex v0(Vec3(0.0f, 0.0f, 0.0f), up, 0.0f, 0.0f);
  Vertex v1(Vec3(1.0f, 0.0f, 0.0f), up, 1.0f, 0.0f);
  Vertex v2(Vec3(0.0f, 0.0f, 1.0f), up, 0.0f, 1.0f);
  t.init(v0, v1, v2, 0);
  return t;
}

static AreaLight makeLight(Triangle& tri, Colour emission) {
  AreaLight light;
  light.triangle = &tri;
  light.emission = emission;
  return light;
}

static ShadingData makeSD(Vec3 wo) {
  ShadingData sd(Vec3(0.0f, 1.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f));
  sd.wo = wo;
  return sd;
}

static const Colour WHITE(1.0f, 1.0f, 1.0f);
static const Colour RED(1.0f, 0.0f, 0.0f);

// -----------------------------------------------------------------------
// isArea
// -----------------------------------------------------------------------

TEST_CASE("AreaLight isArea: returns true") {
  Triangle tri = makeFloorTriangle();
  AreaLight light = makeLight(tri, WHITE);
  REQUIRE(light.isArea() == true);
}

// -----------------------------------------------------------------------
// evaluate — one-sided emission
// -----------------------------------------------------------------------

TEST_CASE("AreaLight evaluate: returns emission for front face (wi antiparallel to normal)") {
  // Triangle normal = (0,1,0). wi pointing down toward it from above → dot < 0 → emission.
  Triangle tri = makeFloorTriangle();
  AreaLight light = makeLight(tri, RED);
  Vec3 wi(0.0f, -1.0f, 0.0f); // from above, pointing toward light
  Colour result = light.evaluate(wi);
  REQUIRE(result.r == Catch::Approx(1.0f));
  REQUIRE(result.g == Catch::Approx(0.0f));
  REQUIRE(result.b == Catch::Approx(0.0f));
}

TEST_CASE("AreaLight evaluate: returns zero for back face (wi parallel to normal)") {
  // wi pointing up, same direction as normal → dot > 0 → back face → black.
  Triangle tri = makeFloorTriangle();
  AreaLight light = makeLight(tri, RED);
  Vec3 wi(0.0f, 1.0f, 0.0f);
  Colour result = light.evaluate(wi);
  REQUIRE(result.r == Catch::Approx(0.0f));
  REQUIRE(result.g == Catch::Approx(0.0f));
  REQUIRE(result.b == Catch::Approx(0.0f));
}

TEST_CASE("AreaLight evaluate: oblique front-face direction returns emission") {
  Triangle tri = makeFloorTriangle();
  AreaLight light = makeLight(tri, WHITE);
  Vec3 wi(0.5f, -0.8f, 0.3f); // mostly downward — front face
  wi = wi.normalize();
  Colour result = light.evaluate(wi);
  REQUIRE(result.r == Catch::Approx(1.0f));
}

// -----------------------------------------------------------------------
// normal
// -----------------------------------------------------------------------

TEST_CASE("AreaLight normal: returns triangle gNormal") {
  Triangle tri = makeFloorTriangle();
  AreaLight light = makeLight(tri, WHITE);
  ShadingData sd = makeSD(Vec3(0.0f, -1.0f, 0.0f));
  Vec3 n = light.normal(sd, Vec3(0.0f, -1.0f, 0.0f));
  REQUIRE(n.x == Catch::Approx(0.0f).margin(1e-5f));
  REQUIRE(n.y == Catch::Approx(1.0f).margin(1e-5f));
  REQUIRE(n.z == Catch::Approx(0.0f).margin(1e-5f));
}

// -----------------------------------------------------------------------
// totalIntegratedPower — area * lum * π (fixed: was missing π)
// -----------------------------------------------------------------------

TEST_CASE("AreaLight totalIntegratedPower: equals area * lum * pi") {
  Triangle tri = makeFloorTriangle();
  AreaLight light = makeLight(tri, WHITE);
  float expected = tri.area * WHITE.lum() * M_PI; // area=0.5, lum=1, π
  REQUIRE(light.totalIntegratedPower() == Catch::Approx(expected).margin(1e-5f));
}

TEST_CASE("AreaLight totalIntegratedPower: scales with emission luminance") {
  Triangle tri = makeFloorTriangle();
  AreaLight dimLight = makeLight(tri, Colour(0.5f, 0.5f, 0.5f));
  AreaLight brightLight = makeLight(tri, Colour(2.0f, 2.0f, 2.0f));
  REQUIRE(brightLight.totalIntegratedPower() ==
          Catch::Approx(dimLight.totalIntegratedPower() * 4.0f).margin(1e-4f));
}

// -----------------------------------------------------------------------
// PDF — returns 1/area (area measure)
// -----------------------------------------------------------------------

TEST_CASE("AreaLight PDF: returns 1/area") {
  Triangle tri = makeFloorTriangle();
  AreaLight light = makeLight(tri, WHITE);
  ShadingData sd = makeSD(Vec3(0.0f, -1.0f, 0.0f));
  float expected = 1.0f / tri.area;
  REQUIRE(light.PDF(sd, Vec3(0.0f, -1.0f, 0.0f)) == Catch::Approx(expected));
}

// -----------------------------------------------------------------------
// sample — emittedColour and pdf
// -----------------------------------------------------------------------

TEST_CASE("AreaLight sample: emittedColour equals emission") {
  Triangle tri = makeFloorTriangle();
  AreaLight light = makeLight(tri, RED);
  ShadingData sd = makeSD(Vec3(0.0f, -1.0f, 0.0f));
  MTRandom sampler;
  Colour emitted; float pdf;
  light.sample(sd, &sampler, emitted, pdf);
  REQUIRE(emitted.r == Catch::Approx(1.0f));
  REQUIRE(emitted.g == Catch::Approx(0.0f));
  REQUIRE(emitted.b == Catch::Approx(0.0f));
}

TEST_CASE("AreaLight sample: pdf equals 1/area") {
  Triangle tri = makeFloorTriangle();
  AreaLight light = makeLight(tri, WHITE);
  ShadingData sd = makeSD(Vec3(0.0f, -1.0f, 0.0f));
  MTRandom sampler;
  float expected = 1.0f / tri.area;
  for (int i = 0; i < 20; i++) {
    Colour emitted; float pdf;
    light.sample(sd, &sampler, emitted, pdf);
    REQUIRE(pdf == Catch::Approx(expected));
  }
}

TEST_CASE("AreaLight sample: pdf matches PDF function") {
  Triangle tri = makeFloorTriangle();
  AreaLight light = makeLight(tri, WHITE);
  ShadingData sd = makeSD(Vec3(0.0f, -1.0f, 0.0f));
  MTRandom sampler;
  for (int i = 0; i < 20; i++) {
    Colour emitted; float samplePdf;
    light.sample(sd, &sampler, emitted, samplePdf);
    float queryPdf = light.PDF(sd, Vec3(0.0f, -1.0f, 0.0f));
    REQUIRE(samplePdf == Catch::Approx(queryPdf));
  }
}

TEST_CASE("AreaLight sample: returned point lies on the triangle") {
  // Triangle: v0=(0,0,0), v1=(1,0,0), v2=(0,0,1). Any point p = α·v0 + β·v1 + γ·v2
  // must satisfy: y=0, x≥0, z≥0, x+z≤1.
  Triangle tri = makeFloorTriangle();
  AreaLight light = makeLight(tri, WHITE);
  ShadingData sd = makeSD(Vec3(0.0f, -1.0f, 0.0f));
  MTRandom sampler;
  for (int i = 0; i < 100; i++) {
    Colour emitted; float pdf;
    Vec3 p = light.sample(sd, &sampler, emitted, pdf);
    REQUIRE(fabsf(p.y) < 1e-5f);
    REQUIRE(p.x >= -1e-5f);
    REQUIRE(p.z >= -1e-5f);
    REQUIRE(p.x + p.z <= 1.0f + 1e-5f);
  }
}

// -----------------------------------------------------------------------
// samplePositionFromLight
// -----------------------------------------------------------------------

TEST_CASE("AreaLight samplePositionFromLight: pdf equals 1/area") {
  Triangle tri = makeFloorTriangle();
  AreaLight light = makeLight(tri, WHITE);
  MTRandom sampler;
  float expected = 1.0f / tri.area;
  for (int i = 0; i < 20; i++) {
    float pdf;
    light.samplePositionFromLight(&sampler, pdf);
    REQUIRE(pdf == Catch::Approx(expected));
  }
}

TEST_CASE("AreaLight samplePositionFromLight: returned point lies on the triangle") {
  Triangle tri = makeFloorTriangle();
  AreaLight light = makeLight(tri, WHITE);
  MTRandom sampler;
  for (int i = 0; i < 100; i++) {
    float pdf;
    Vec3 p = light.samplePositionFromLight(&sampler, pdf);
    REQUIRE(fabsf(p.y) < 1e-5f);
    REQUIRE(p.x >= -1e-5f);
    REQUIRE(p.z >= -1e-5f);
    REQUIRE(p.x + p.z <= 1.0f + 1e-5f);
  }
}

// -----------------------------------------------------------------------
// sampleDirectionFromLight — cosine-weighted from light's hemisphere
// -----------------------------------------------------------------------

TEST_CASE("AreaLight sampleDirectionFromLight: direction is in front hemisphere of light") {
  // Normal = (0,1,0). All sampled directions must have positive y component.
  Triangle tri = makeFloorTriangle();
  AreaLight light = makeLight(tri, WHITE);
  MTRandom sampler;
  for (int i = 0; i < 100; i++) {
    float pdf;
    Vec3 wi = light.sampleDirectionFromLight(&sampler, pdf);
    REQUIRE(wi.y > -1e-5f); // in upper hemisphere relative to light normal
  }
}

TEST_CASE("AreaLight sampleDirectionFromLight: pdf is non-negative") {
  Triangle tri = makeFloorTriangle();
  AreaLight light = makeLight(tri, WHITE);
  MTRandom sampler;
  for (int i = 0; i < 50; i++) {
    float pdf;
    light.sampleDirectionFromLight(&sampler, pdf);
    REQUIRE(pdf >= 0.0f);
  }
}

// -----------------------------------------------------------------------
// Edge cases
// -----------------------------------------------------------------------

TEST_CASE("AreaLight evaluate: grazing angle (dot == 0) returns zero") {
  // Exactly tangent to the surface — sits on the boundary of the front/back condition.
  // The condition is dot < 0, so dot == 0 must return black (not emission).
  Triangle tri = makeFloorTriangle();
  AreaLight light = makeLight(tri, RED);
  Vec3 wi(1.0f, 0.0f, 0.0f); // tangent to gNormal=(0,1,0) → dot == 0
  Colour result = light.evaluate(wi);
  REQUIRE(result.r == Catch::Approx(0.0f));
  REQUIRE(result.g == Catch::Approx(0.0f));
  REQUIRE(result.b == Catch::Approx(0.0f));
}

TEST_CASE("AreaLight totalIntegratedPower: zero emission gives zero power") {
  Triangle tri = makeFloorTriangle();
  AreaLight light = makeLight(tri, Colour(0.0f, 0.0f, 0.0f));
  REQUIRE(light.totalIntegratedPower() == Catch::Approx(0.0f));
}

TEST_CASE("AreaLight PDF: direction-independent, same value for any wi") {
  Triangle tri = makeFloorTriangle();
  AreaLight light = makeLight(tri, WHITE);
  ShadingData sd = makeSD(Vec3(0.0f, -1.0f, 0.0f));
  float expected = 1.0f / tri.area;
  // PDF ignores wi entirely — verify it returns the same value for varied directions
  REQUIRE(light.PDF(sd, Vec3(0.0f, -1.0f,  0.0f)) == Catch::Approx(expected));
  REQUIRE(light.PDF(sd, Vec3(1.0f,  0.0f,  0.0f)) == Catch::Approx(expected));
  REQUIRE(light.PDF(sd, Vec3(0.0f,  1.0f,  0.0f)) == Catch::Approx(expected));
  REQUIRE(light.PDF(sd, Vec3(0.5f, -0.5f,  0.5f)) == Catch::Approx(expected));
}

TEST_CASE("AreaLight sampleDirectionFromLight: returned direction is unit length") {
  Triangle tri = makeFloorTriangle();
  AreaLight light = makeLight(tri, WHITE);
  MTRandom sampler;
  for (int i = 0; i < 50; i++) {
    float pdf;
    Vec3 wi = light.sampleDirectionFromLight(&sampler, pdf);
    float len = sqrtf(wi.x*wi.x + wi.y*wi.y + wi.z*wi.z);
    REQUIRE(len == Catch::Approx(1.0f).margin(1e-4f));
  }
}
