#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "texture.h"

// -----------------------------------------------------------------------
// Constructor
// -----------------------------------------------------------------------

TEST_CASE("Texture constructor: members initialised to zero/null") {
  Texture t;
  REQUIRE(t.texels   == nullptr);
  REQUIRE(t.alpha    == nullptr);
  REQUIRE(t.width    == 0);
  REQUIRE(t.height   == 0);
  REQUIRE(t.channels == 0);
}

// -----------------------------------------------------------------------
// loadDefault
// -----------------------------------------------------------------------

TEST_CASE("Texture loadDefault: creates 1x1 texture") {
  Texture t;
  t.loadDefault();
  REQUIRE(t.width  == 1);
  REQUIRE(t.height == 1);
}

TEST_CASE("Texture loadDefault: sample returns white") {
  Texture t;
  t.loadDefault();
  Colour c = t.sample(0.0f, 0.0f);
  REQUIRE(c.r == Catch::Approx(1.0f));
  REQUIRE(c.g == Catch::Approx(1.0f));
  REQUIRE(c.b == Catch::Approx(1.0f));
}

TEST_CASE("Texture loadDefault: sampleAlpha returns 1.0 (no alpha channel)") {
  Texture t;
  t.loadDefault();
  REQUIRE(t.sampleAlpha(0.0f, 0.0f) == Catch::Approx(1.0f));
}

// -----------------------------------------------------------------------
// load — invalid path falls back to loadDefault
// -----------------------------------------------------------------------

TEST_CASE("Texture load: invalid filename falls back to 1x1 white") {
  Texture t;
  t.load("this_file_does_not_exist.png");
  REQUIRE(t.width  == 1);
  REQUIRE(t.height == 1);
  Colour c = t.sample(0.0f, 0.0f);
  REQUIRE(c.r == Catch::Approx(1.0f));
  REQUIRE(c.g == Catch::Approx(1.0f));
  REQUIRE(c.b == Catch::Approx(1.0f));
}

// -----------------------------------------------------------------------
// sample — bilinear interpolation
// Helper: build a 2x1 texture manually (red at x=0, green at x=1)
// -----------------------------------------------------------------------

static Texture make2x1() {
  Texture t;
  t.width    = 2;
  t.height   = 1;
  t.channels = 3;
  t.texels   = new Colour[2];
  t.texels[0] = Colour(1.0f, 0.0f, 0.0f); // red
  t.texels[1] = Colour(0.0f, 1.0f, 0.0f); // green
  return t;
}

TEST_CASE("Texture sample: tu=0 returns first texel exactly") {
  Texture t = make2x1();
  Colour c = t.sample(0.0f, 0.0f);
  REQUIRE(c.r == Catch::Approx(1.0f));
  REQUIRE(c.g == Catch::Approx(0.0f));
  REQUIRE(c.b == Catch::Approx(0.0f));
}

TEST_CASE("Texture sample: tu=0.5 returns second texel exactly") {
  Texture t = make2x1();
  Colour c = t.sample(0.5f, 0.0f);
  REQUIRE(c.r == Catch::Approx(0.0f));
  REQUIRE(c.g == Catch::Approx(1.0f));
  REQUIRE(c.b == Catch::Approx(0.0f));
}

TEST_CASE("Texture sample: tu=0.25 returns 50/50 blend of first and second texel") {
  Texture t = make2x1();
  Colour c = t.sample(0.25f, 0.0f);
  REQUIRE(c.r == Catch::Approx(0.5f).margin(0.001f));
  REQUIRE(c.g == Catch::Approx(0.5f).margin(0.001f));
  REQUIRE(c.b == Catch::Approx(0.0f));
}

TEST_CASE("Texture sample: tu=1.0 wraps back to first texel") {
  Texture t = make2x1();
  Colour c = t.sample(1.0f, 0.0f);
  REQUIRE(c.r == Catch::Approx(1.0f));
  REQUIRE(c.g == Catch::Approx(0.0f));
}

// -----------------------------------------------------------------------
// sample — 2x2, verifies bilinear across both axes
// -----------------------------------------------------------------------

static Texture make2x2() {
  Texture t;
  t.width    = 2;
  t.height   = 2;
  t.channels = 3;
  t.texels   = new Colour[4];
  t.texels[0] = Colour(1.0f, 0.0f, 0.0f); // x=0 y=0  red
  t.texels[1] = Colour(0.0f, 1.0f, 0.0f); // x=1 y=0  green
  t.texels[2] = Colour(0.0f, 0.0f, 1.0f); // x=0 y=1  blue
  t.texels[3] = Colour(1.0f, 1.0f, 0.0f); // x=1 y=1  yellow
  return t;
}

TEST_CASE("Texture sample: 2x2 center gives equal blend of all four texels") {
  // tu=0.25, tv=0.25 → frac_u=0.5, frac_v=0.5 → w0=w1=w2=w3=0.25
  // result = 0.25*(1,0,0) + 0.25*(0,1,0) + 0.25*(0,0,1) + 0.25*(1,1,0)
  //        = (0.5, 0.5, 0.25)
  Texture t = make2x2();
  Colour c = t.sample(0.25f, 0.25f);
  REQUIRE(c.r == Catch::Approx(0.5f).margin(0.001f));
  REQUIRE(c.g == Catch::Approx(0.5f).margin(0.001f));
  REQUIRE(c.b == Catch::Approx(0.25f).margin(0.001f));
}

TEST_CASE("Texture sample: 2x2 top-left corner returns red texel") {
  Texture t = make2x2();
  Colour c = t.sample(0.0f, 0.0f);
  REQUIRE(c.r == Catch::Approx(1.0f));
  REQUIRE(c.g == Catch::Approx(0.0f));
  REQUIRE(c.b == Catch::Approx(0.0f));
}

// -----------------------------------------------------------------------
// sampleAlpha
// -----------------------------------------------------------------------

TEST_CASE("Texture sampleAlpha: returns 1.0 when no alpha channel") {
  Texture t = make2x1();
  REQUIRE(t.sampleAlpha(0.0f, 0.0f) == Catch::Approx(1.0f));
  REQUIRE(t.sampleAlpha(0.5f, 0.0f) == Catch::Approx(1.0f));
}

TEST_CASE("Texture sampleAlpha: returns correct value at known texel") {
  Texture t = make2x1();
  t.alpha    = new float[2];
  t.alpha[0] = 0.0f;
  t.alpha[1] = 1.0f;

  REQUIRE(t.sampleAlpha(0.0f, 0.0f) == Catch::Approx(0.0f));
  REQUIRE(t.sampleAlpha(0.5f, 0.0f) == Catch::Approx(1.0f));
}

TEST_CASE("Texture sampleAlpha: midpoint blend of two alpha values") {
  Texture t = make2x1();
  t.alpha    = new float[2];
  t.alpha[0] = 0.0f;
  t.alpha[1] = 1.0f;

  // tu=0.25 → frac_u=0.5 → 50/50 blend → 0.5
  REQUIRE(t.sampleAlpha(0.25f, 0.0f) == Catch::Approx(0.5f).margin(0.001f));
}
