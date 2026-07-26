#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "light.h"
#include "sampling.h"
#include <cmath>

static ShadingData makeSD() {
  ShadingData sd(Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f));
  sd.wo = Vec3(0.0f, 1.0f, 0.0f);
  return sd;
}

static const Colour WHITE(1.0f, 1.0f, 1.0f);
static const Colour RED(1.0f, 0.0f, 0.0f);
static const float SPHERE_PDF = 1.0f / (4.0f * M_PI);

// -----------------------------------------------------------------------
// isArea
// -----------------------------------------------------------------------

TEST_CASE("BackgroundColour isArea: returns false") {
  BackgroundColour light(WHITE);
  REQUIRE(light.isArea() == false);
}

// -----------------------------------------------------------------------
// evaluate — constant emission in all directions
// -----------------------------------------------------------------------

TEST_CASE("BackgroundColour evaluate: returns emission for any direction") {
  BackgroundColour light(RED);
  REQUIRE(light.evaluate(Vec3( 1.0f,  0.0f,  0.0f)).r == Catch::Approx(1.0f));
  REQUIRE(light.evaluate(Vec3(-1.0f,  0.0f,  0.0f)).r == Catch::Approx(1.0f));
  REQUIRE(light.evaluate(Vec3( 0.0f,  1.0f,  0.0f)).r == Catch::Approx(1.0f));
  REQUIRE(light.evaluate(Vec3( 0.0f, -1.0f,  0.0f)).r == Catch::Approx(1.0f));
  REQUIRE(light.evaluate(Vec3( 0.0f,  0.0f,  1.0f)).r == Catch::Approx(1.0f));
  REQUIRE(light.evaluate(Vec3( 0.0f,  0.0f, -1.0f)).r == Catch::Approx(1.0f));
}

TEST_CASE("BackgroundColour evaluate: zero emission returns black") {
  BackgroundColour light(Colour(0.0f, 0.0f, 0.0f));
  Colour result = light.evaluate(Vec3(0.0f, 1.0f, 0.0f));
  REQUIRE(result.r == Catch::Approx(0.0f));
  REQUIRE(result.g == Catch::Approx(0.0f));
  REQUIRE(result.b == Catch::Approx(0.0f));
}

// -----------------------------------------------------------------------
// normal — returns -wi (inward-pointing for infinite background)
// -----------------------------------------------------------------------

TEST_CASE("BackgroundColour normal: returns -wi") {
  BackgroundColour light(WHITE);
  ShadingData sd = makeSD();
  Vec3 wi(0.5f, 0.7f, -0.3f);
  wi = wi.normalize();
  Vec3 n = light.normal(sd, wi);
  REQUIRE(n.x == Catch::Approx(-wi.x).margin(1e-5f));
  REQUIRE(n.y == Catch::Approx(-wi.y).margin(1e-5f));
  REQUIRE(n.z == Catch::Approx(-wi.z).margin(1e-5f));
}

// -----------------------------------------------------------------------
// totalIntegratedPower — emission.lum() * 4π
// -----------------------------------------------------------------------

TEST_CASE("BackgroundColour totalIntegratedPower: equals lum * 4pi") {
  BackgroundColour light(WHITE);
  float expected = WHITE.lum() * 4.0f * M_PI;
  REQUIRE(light.totalIntegratedPower() == Catch::Approx(expected).margin(1e-4f));
}

TEST_CASE("BackgroundColour totalIntegratedPower: zero emission gives zero power") {
  BackgroundColour light(Colour(0.0f, 0.0f, 0.0f));
  REQUIRE(light.totalIntegratedPower() == Catch::Approx(0.0f));
}

TEST_CASE("BackgroundColour totalIntegratedPower: scales linearly with emission") {
  BackgroundColour dim(Colour(0.5f, 0.5f, 0.5f));
  BackgroundColour bright(Colour(2.0f, 2.0f, 2.0f));
  REQUIRE(bright.totalIntegratedPower() ==
          Catch::Approx(dim.totalIntegratedPower() * 4.0f).margin(1e-4f));
}

// -----------------------------------------------------------------------
// PDF — constant 1/(4π) regardless of direction or shading data
// -----------------------------------------------------------------------

TEST_CASE("BackgroundColour PDF: returns 1/(4pi)") {
  BackgroundColour light(WHITE);
  ShadingData sd = makeSD();
  REQUIRE(light.PDF(sd, Vec3(1.0f, 0.0f, 0.0f)) == Catch::Approx(SPHERE_PDF).margin(1e-6f));
}

TEST_CASE("BackgroundColour PDF: direction-independent") {
  BackgroundColour light(WHITE);
  ShadingData sd = makeSD();
  REQUIRE(light.PDF(sd, Vec3( 1.0f,  0.0f,  0.0f)) == Catch::Approx(SPHERE_PDF).margin(1e-6f));
  REQUIRE(light.PDF(sd, Vec3( 0.0f, -1.0f,  0.0f)) == Catch::Approx(SPHERE_PDF).margin(1e-6f));
  REQUIRE(light.PDF(sd, Vec3( 0.5f,  0.5f,  0.5f)) == Catch::Approx(SPHERE_PDF).margin(1e-6f));
}

// -----------------------------------------------------------------------
// sample — emittedColour, pdf, and direction
// -----------------------------------------------------------------------

TEST_CASE("BackgroundColour sample: emittedColour equals emission") {
  BackgroundColour light(RED);
  ShadingData sd = makeSD();
  MTRandom sampler;
  for (int i = 0; i < 20; i++) {
    Colour emitted; float pdf;
    light.sample(sd, &sampler, emitted, pdf);
    REQUIRE(emitted.r == Catch::Approx(1.0f));
    REQUIRE(emitted.g == Catch::Approx(0.0f));
    REQUIRE(emitted.b == Catch::Approx(0.0f));
  }
}

TEST_CASE("BackgroundColour sample: pdf equals 1/(4pi)") {
  BackgroundColour light(WHITE);
  ShadingData sd = makeSD();
  MTRandom sampler;
  for (int i = 0; i < 50; i++) {
    Colour emitted; float pdf;
    light.sample(sd, &sampler, emitted, pdf);
    REQUIRE(pdf == Catch::Approx(SPHERE_PDF).margin(1e-6f));
  }
}

TEST_CASE("BackgroundColour sample: pdf matches PDF function") {
  BackgroundColour light(WHITE);
  ShadingData sd = makeSD();
  MTRandom sampler;
  for (int i = 0; i < 50; i++) {
    Colour emitted; float samplePdf;
    Vec3 wi = light.sample(sd, &sampler, emitted, samplePdf);
    float queryPdf = light.PDF(sd, wi);
    REQUIRE(samplePdf == Catch::Approx(queryPdf).margin(1e-6f));
  }
}

TEST_CASE("BackgroundColour sample: returned direction is unit length") {
  BackgroundColour light(WHITE);
  ShadingData sd = makeSD();
  MTRandom sampler;
  for (int i = 0; i < 50; i++) {
    Colour emitted; float pdf;
    Vec3 wi = light.sample(sd, &sampler, emitted, pdf);
    float len = sqrtf(wi.x*wi.x + wi.y*wi.y + wi.z*wi.z);
    REQUIRE(len == Catch::Approx(1.0f).margin(1e-4f));
  }
}

// -----------------------------------------------------------------------
// sampleDirectionFromLight
// -----------------------------------------------------------------------

TEST_CASE("BackgroundColour sampleDirectionFromLight: pdf equals 1/(4pi)") {
  BackgroundColour light(WHITE);
  MTRandom sampler;
  for (int i = 0; i < 50; i++) {
    float pdf;
    light.sampleDirectionFromLight(&sampler, pdf);
    REQUIRE(pdf == Catch::Approx(SPHERE_PDF).margin(1e-6f));
  }
}

TEST_CASE("BackgroundColour sampleDirectionFromLight: covers full sphere (directions in both hemispheres)") {
  BackgroundColour light(WHITE);
  MTRandom sampler;
  bool foundPositiveY = false, foundNegativeY = false;
  for (int i = 0; i < 200; i++) {
    float pdf;
    Vec3 wi = light.sampleDirectionFromLight(&sampler, pdf);
    if (wi.y >  0.1f) foundPositiveY = true;
    if (wi.y < -0.1f) foundNegativeY = true;
    if (foundPositiveY && foundNegativeY) break;
  }
  REQUIRE(foundPositiveY);
  REQUIRE(foundNegativeY);
}

TEST_CASE("BackgroundColour sampleDirectionFromLight: returned direction is unit length") {
  BackgroundColour light(WHITE);
  MTRandom sampler;
  for (int i = 0; i < 50; i++) {
    float pdf;
    Vec3 wi = light.sampleDirectionFromLight(&sampler, pdf);
    float len = sqrtf(wi.x*wi.x + wi.y*wi.y + wi.z*wi.z);
    REQUIRE(len == Catch::Approx(1.0f).margin(1e-4f));
  }
}

TEST_CASE("BackgroundColour sample: covers full sphere (directions in both hemispheres)") {
  // A hemisphere sampler would never produce wi.y < 0 for a y-up normal.
  // Over enough samples both signs must appear.
  BackgroundColour light(WHITE);
  ShadingData sd = makeSD();
  MTRandom sampler;
  bool foundPositiveY = false, foundNegativeY = false;
  for (int i = 0; i < 200; i++) {
    Colour emitted; float pdf;
    Vec3 wi = light.sample(sd, &sampler, emitted, pdf);
    if (wi.y >  0.1f) foundPositiveY = true;
    if (wi.y < -0.1f) foundNegativeY = true;
    if (foundPositiveY && foundNegativeY) break;
  }
  REQUIRE(foundPositiveY);
  REQUIRE(foundNegativeY);
}

// -----------------------------------------------------------------------
// samplePositionFromLight — inherited stub, not supported
// -----------------------------------------------------------------------

TEST_CASE("BackgroundColour samplePositionFromLight: returns pdf=0 (not supported)") {
  BackgroundColour light(WHITE);
  MTRandom sampler;
  float pdf;
  light.samplePositionFromLight(&sampler, pdf);
  REQUIRE(pdf == Catch::Approx(0.0f));
}
