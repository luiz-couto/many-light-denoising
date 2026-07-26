#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "light.h"
#include "texture.h"
#include "sampling.h"
#include <cmath>

// -----------------------------------------------------------------------
// Texture helpers
// -----------------------------------------------------------------------

// Allocates a WxH texture where every pixel has the given colour.
// Texture destructor handles delete[], so no manual cleanup needed.
static void fillTexture(Texture& tex, int w, int h, Colour c) {
  tex.width    = w;
  tex.height   = h;
  tex.channels = 3;
  tex.texels   = new Colour[w * h];
  tex.alpha    = nullptr;
  for (int i = 0; i < w * h; i++) tex.texels[i] = c;
}

// 4x4 uniform white texture — equatorial pixels have non-zero sin(θ) weight.
static void makeUniform(Texture& tex) { fillTexture(tex, 4, 4, Colour(1.0f, 1.0f, 1.0f)); }

// 4x4 black texture — totalSum = 0.
static void makeBlack(Texture& tex) { fillTexture(tex, 4, 4, Colour(0.0f, 0.0f, 0.0f)); }

// Single bright pixel at equatorial centre (u=2, v=2 in a 4x4 map),
// all other pixels black. That pixel maps to theta=π/2, phi=π → wi=(-1,0,0).
static void makeSingleBright(Texture& tex) {
  fillTexture(tex, 4, 4, Colour(0.0f, 0.0f, 0.0f));
  tex.texels[2 * 4 + 2] = Colour(1.0f, 1.0f, 1.0f);
}

static ShadingData makeSD() {
  ShadingData sd(Vec3(0,0,0), Vec3(0,1,0));
  sd.wo = Vec3(0,1,0);
  return sd;
}

static const Vec3  CENTRE(0.0f, 0.0f, 0.0f);
static const float RADIUS = 10.0f;

// -----------------------------------------------------------------------
// isArea
// -----------------------------------------------------------------------

TEST_CASE("EnvironmentMap isArea: returns false") {
  Texture tex; makeUniform(tex);
  EnvironmentMap env(&tex, CENTRE, RADIUS);
  REQUIRE(env.isArea() == false);
}

// -----------------------------------------------------------------------
// evaluate — UV mapping and NaN guards
// -----------------------------------------------------------------------

TEST_CASE("EnvironmentMap evaluate: returns non-NaN for all axis directions") {
  Texture tex; makeUniform(tex);
  EnvironmentMap env(&tex, CENTRE, RADIUS);
  Vec3 dirs[] = {
    Vec3( 1,0,0), Vec3(-1,0,0),
    Vec3(0, 1,0), Vec3(0,-1,0),
    Vec3(0,0, 1), Vec3(0,0,-1)
  };
  for (auto& wi : dirs) {
    Colour c = env.evaluate(wi);
    REQUIRE(std::isfinite(c.r));
    REQUIRE(std::isfinite(c.g));
    REQUIRE(std::isfinite(c.b));
  }
}

TEST_CASE("EnvironmentMap evaluate: clamps wi.y > 1 — no NaN (pole guard)") {
  Texture tex; makeUniform(tex);
  EnvironmentMap env(&tex, CENTRE, RADIUS);
  Colour c = env.evaluate(Vec3(0.0f, 1.001f, 0.0f));
  REQUIRE(std::isfinite(c.r));
}

TEST_CASE("EnvironmentMap evaluate: clamps wi.y < -1 — no NaN (pole guard)") {
  Texture tex; makeUniform(tex);
  EnvironmentMap env(&tex, CENTRE, RADIUS);
  Colour c = env.evaluate(Vec3(0.0f, -1.001f, 0.0f));
  REQUIRE(std::isfinite(c.r));
}

TEST_CASE("EnvironmentMap evaluate: wi.y exactly 1.0 (north pole) — no NaN") {
  Texture tex; makeUniform(tex);
  EnvironmentMap env(&tex, CENTRE, RADIUS);
  Colour c = env.evaluate(Vec3(0.0f, 1.0f, 0.0f));
  REQUIRE(std::isfinite(c.r));
}

TEST_CASE("EnvironmentMap evaluate: wi.y exactly -1.0 (south pole) — no NaN") {
  Texture tex; makeUniform(tex);
  EnvironmentMap env(&tex, CENTRE, RADIUS);
  Colour c = env.evaluate(Vec3(0.0f, -1.0f, 0.0f));
  REQUIRE(std::isfinite(c.r));
}

// -----------------------------------------------------------------------
// PDF — correctness and edge cases
// -----------------------------------------------------------------------

TEST_CASE("EnvironmentMap PDF: non-negative for all directions") {
  Texture tex; makeUniform(tex);
  EnvironmentMap env(&tex, CENTRE, RADIUS);
  ShadingData sd = makeSD();
  MTRandom sampler;
  for (int i = 0; i < 100; i++) {
    Vec3 wi = SamplingDistributions::uniformSampleSphere(sampler.next(), sampler.next());
    REQUIRE(env.PDF(sd, wi) >= 0.0f);
  }
}

TEST_CASE("EnvironmentMap PDF: direction-independent for uniform texture") {
  // Every direction maps to the same luminance, so pdf must be equal everywhere.
  Texture tex; makeUniform(tex);
  EnvironmentMap env(&tex, CENTRE, RADIUS);
  ShadingData sd = makeSD();
  float ref = env.PDF(sd, Vec3(1.0f, 0.0f, 0.0f));
  Vec3 dirs[] = {
    Vec3(-1,0,0), Vec3(0,0,1), Vec3(0,0,-1),
    Vec3(0.577f, 0.577f, 0.577f)
  };
  for (auto& wi : dirs) {
    REQUIRE(env.PDF(sd, wi.normalize()) == Catch::Approx(ref).margin(1e-4f));
  }
}

TEST_CASE("EnvironmentMap PDF: at north pole — no NaN (clamp fix)") {
  Texture tex; makeUniform(tex);
  EnvironmentMap env(&tex, CENTRE, RADIUS);
  ShadingData sd = makeSD();
  float pdf = env.PDF(sd, Vec3(0.0f, 1.0f, 0.0f));
  REQUIRE(std::isfinite(pdf));
  REQUIRE(pdf >= 0.0f);
}

TEST_CASE("EnvironmentMap PDF: at south pole — no NaN (clamp fix)") {
  Texture tex; makeUniform(tex);
  EnvironmentMap env(&tex, CENTRE, RADIUS);
  ShadingData sd = makeSD();
  float pdf = env.PDF(sd, Vec3(0.0f, -1.0f, 0.0f));
  REQUIRE(std::isfinite(pdf));
  REQUIRE(pdf >= 0.0f);
}

TEST_CASE("EnvironmentMap PDF: wi.y slightly outside [-1,1] — no NaN (clamp fix)") {
  Texture tex; makeUniform(tex);
  EnvironmentMap env(&tex, CENTRE, RADIUS);
  ShadingData sd = makeSD();
  REQUIRE(std::isfinite(env.PDF(sd, Vec3(0.0f,  1.002f, 0.0f))));
  REQUIRE(std::isfinite(env.PDF(sd, Vec3(0.0f, -1.002f, 0.0f))));
}

TEST_CASE("EnvironmentMap PDF: fully black texture returns 0, not NaN") {
  Texture tex; makeBlack(tex);
  EnvironmentMap env(&tex, CENTRE, RADIUS);
  ShadingData sd = makeSD();
  float pdf = env.PDF(sd, Vec3(1.0f, 0.0f, 0.0f));
  REQUIRE(std::isfinite(pdf));
  REQUIRE(pdf == Catch::Approx(0.0f));
}

// -----------------------------------------------------------------------
// sample — validity and importance sampling
// -----------------------------------------------------------------------

TEST_CASE("EnvironmentMap sample: pdf non-negative") {
  Texture tex; makeUniform(tex);
  EnvironmentMap env(&tex, CENTRE, RADIUS);
  ShadingData sd = makeSD(); MTRandom sampler;
  for (int i = 0; i < 100; i++) {
    Colour col; float pdf;
    env.sample(sd, &sampler, col, pdf);
    REQUIRE(pdf >= 0.0f);
  }
}

TEST_CASE("EnvironmentMap sample: returned direction is unit length") {
  Texture tex; makeUniform(tex);
  EnvironmentMap env(&tex, CENTRE, RADIUS);
  ShadingData sd = makeSD(); MTRandom sampler;
  for (int i = 0; i < 50; i++) {
    Colour col; float pdf;
    Vec3 wi = env.sample(sd, &sampler, col, pdf);
    float len = sqrtf(wi.x*wi.x + wi.y*wi.y + wi.z*wi.z);
    REQUIRE(len == Catch::Approx(1.0f).margin(1e-4f));
  }
}

TEST_CASE("EnvironmentMap sample: emittedColour equals evaluate(wi)") {
  Texture tex; makeUniform(tex);
  EnvironmentMap env(&tex, CENTRE, RADIUS);
  ShadingData sd = makeSD(); MTRandom sampler;
  for (int i = 0; i < 50; i++) {
    Colour col; float pdf;
    Vec3 wi = env.sample(sd, &sampler, col, pdf);
    Colour expected = env.evaluate(wi);
    REQUIRE(col.r == Catch::Approx(expected.r).margin(1e-4f));
    REQUIRE(col.g == Catch::Approx(expected.g).margin(1e-4f));
    REQUIRE(col.b == Catch::Approx(expected.b).margin(1e-4f));
  }
}

TEST_CASE("EnvironmentMap sample: pdf matches PDF function") {
  Texture tex; makeUniform(tex);
  EnvironmentMap env(&tex, CENTRE, RADIUS);
  ShadingData sd = makeSD(); MTRandom sampler;
  for (int i = 0; i < 50; i++) {
    Colour col; float samplePdf;
    Vec3 wi = env.sample(sd, &sampler, col, samplePdf);
    float queryPdf = env.PDF(sd, wi);
    REQUIRE(samplePdf == Catch::Approx(queryPdf).margin(1e-4f));
  }
}

TEST_CASE("EnvironmentMap sample: covers full sphere (both hemispheres)") {
  Texture tex; makeUniform(tex);
  EnvironmentMap env(&tex, CENTRE, RADIUS);
  ShadingData sd = makeSD(); MTRandom sampler;
  bool foundPos = false, foundNeg = false;
  for (int i = 0; i < 200; i++) {
    Colour col; float pdf;
    Vec3 wi = env.sample(sd, &sampler, col, pdf);
    if (wi.y >  0.1f) foundPos = true;
    if (wi.y < -0.1f) foundNeg = true;
    if (foundPos && foundNeg) break;
  }
  REQUIRE(foundPos);
  REQUIRE(foundNeg);
}

TEST_CASE("EnvironmentMap sample: importance sampling concentrates near bright pixel") {
  // Single bright pixel maps to wi ≈ (-1, 0, 0). With all other pixels black,
  // all samples must land at that direction.
  Texture tex; makeSingleBright(tex);
  EnvironmentMap env(&tex, CENTRE, RADIUS);
  ShadingData sd = makeSD(); MTRandom sampler;
  int nearBright = 0;
  for (int i = 0; i < 100; i++) {
    Colour col; float pdf;
    Vec3 wi = env.sample(sd, &sampler, col, pdf);
    if (wi.x < -0.5f) nearBright++;
  }
  REQUIRE(nearBright > 90); // virtually all samples should land near the bright pixel
}

// -----------------------------------------------------------------------
// sampleDirectionFromLight
// -----------------------------------------------------------------------

TEST_CASE("EnvironmentMap sampleDirectionFromLight: returned direction is unit length") {
  Texture tex; makeUniform(tex);
  EnvironmentMap env(&tex, CENTRE, RADIUS);
  MTRandom sampler;
  for (int i = 0; i < 50; i++) {
    float pdf;
    Vec3 wi = env.sampleDirectionFromLight(&sampler, pdf);
    float len = sqrtf(wi.x*wi.x + wi.y*wi.y + wi.z*wi.z);
    REQUIRE(len == Catch::Approx(1.0f).margin(1e-4f));
  }
}

TEST_CASE("EnvironmentMap sampleDirectionFromLight: pdf non-negative") {
  Texture tex; makeUniform(tex);
  EnvironmentMap env(&tex, CENTRE, RADIUS);
  MTRandom sampler;
  for (int i = 0; i < 50; i++) {
    float pdf;
    env.sampleDirectionFromLight(&sampler, pdf);
    REQUIRE(pdf >= 0.0f);
  }
}

TEST_CASE("EnvironmentMap sampleDirectionFromLight: pdf matches PDF function") {
  Texture tex; makeUniform(tex);
  EnvironmentMap env(&tex, CENTRE, RADIUS);
  ShadingData sd = makeSD(); MTRandom sampler;
  for (int i = 0; i < 50; i++) {
    float samplePdf;
    Vec3 wi = env.sampleDirectionFromLight(&sampler, samplePdf);
    float queryPdf = env.PDF(sd, wi);
    REQUIRE(samplePdf == Catch::Approx(queryPdf).margin(1e-4f));
  }
}

TEST_CASE("EnvironmentMap sampleDirectionFromLight: covers full sphere") {
  Texture tex; makeUniform(tex);
  EnvironmentMap env(&tex, CENTRE, RADIUS);
  MTRandom sampler;
  bool foundPos = false, foundNeg = false;
  for (int i = 0; i < 200; i++) {
    float pdf;
    Vec3 wi = env.sampleDirectionFromLight(&sampler, pdf);
    if (wi.y >  0.1f) foundPos = true;
    if (wi.y < -0.1f) foundNeg = true;
    if (foundPos && foundNeg) break;
  }
  REQUIRE(foundPos);
  REQUIRE(foundNeg);
}

TEST_CASE("EnvironmentMap sampleDirectionFromLight: black texture returns pdf=0, no crash") {
  Texture tex; makeBlack(tex);
  EnvironmentMap env(&tex, CENTRE, RADIUS);
  MTRandom sampler;
  float pdf;
  env.sampleDirectionFromLight(&sampler, pdf);
  REQUIRE(std::isfinite(pdf));
  REQUIRE(pdf == Catch::Approx(0.0f));
}

// -----------------------------------------------------------------------
// samplePositionFromLight
// -----------------------------------------------------------------------

TEST_CASE("EnvironmentMap samplePositionFromLight: point lies on bounding sphere") {
  Texture tex; makeUniform(tex);
  EnvironmentMap env(&tex, CENTRE, RADIUS);
  MTRandom sampler;
  for (int i = 0; i < 50; i++) {
    float pdf;
    Vec3 p = env.samplePositionFromLight(&sampler, pdf);
    Vec3 d = p - CENTRE;
    float r = sqrtf(d.x*d.x + d.y*d.y + d.z*d.z);
    REQUIRE(r == Catch::Approx(RADIUS).margin(1e-3f));
  }
}

TEST_CASE("EnvironmentMap samplePositionFromLight: pdf equals 1/(4pi*r^2)") {
  Texture tex; makeUniform(tex);
  EnvironmentMap env(&tex, CENTRE, RADIUS);
  MTRandom sampler;
  float expected = 1.0f / (4.0f * M_PI * RADIUS * RADIUS);
  for (int i = 0; i < 20; i++) {
    float pdf;
    env.samplePositionFromLight(&sampler, pdf);
    REQUIRE(pdf == Catch::Approx(expected).margin(1e-6f));
  }
}

// -----------------------------------------------------------------------
// totalIntegratedPower
// -----------------------------------------------------------------------

TEST_CASE("EnvironmentMap totalIntegratedPower: non-negative") {
  Texture tex; makeUniform(tex);
  EnvironmentMap env(&tex, CENTRE, RADIUS);
  REQUIRE(env.totalIntegratedPower() >= 0.0f);
}

TEST_CASE("EnvironmentMap totalIntegratedPower: zero for black texture") {
  Texture tex; makeBlack(tex);
  EnvironmentMap env(&tex, CENTRE, RADIUS);
  REQUIRE(env.totalIntegratedPower() == Catch::Approx(0.0f));
}

TEST_CASE("EnvironmentMap totalIntegratedPower: cached — same value on repeated calls") {
  Texture tex; makeUniform(tex);
  EnvironmentMap env(&tex, CENTRE, RADIUS);
  float first  = env.totalIntegratedPower();
  float second = env.totalIntegratedPower();
  REQUIRE(first == Catch::Approx(second));
}

TEST_CASE("EnvironmentMap totalIntegratedPower: scales with emission brightness") {
  Texture dim; fillTexture(dim, 4, 4, Colour(0.5f, 0.5f, 0.5f));
  Texture bright; fillTexture(bright, 4, 4, Colour(2.0f, 2.0f, 2.0f));
  EnvironmentMap eDim(&dim, CENTRE, RADIUS);
  EnvironmentMap eBright(&bright, CENTRE, RADIUS);
  REQUIRE(eBright.totalIntegratedPower() ==
          Catch::Approx(eDim.totalIntegratedPower() * 4.0f).margin(1e-4f));
}
