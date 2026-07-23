#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "film.h"

static Film makeFilm(int w = 4, int h = 4) {
  Film f;
  f.init(w, h);
  return f;
}

// -----------------------------------------------------------------------
// init
// -----------------------------------------------------------------------

TEST_CASE("Film init: stores width and height") {
  Film f = makeFilm(8, 6);
  REQUIRE(f.width  == 8);
  REQUIRE(f.height == 6);
}

TEST_CASE("Film init: film buffer sized to width * height") {
  Film f = makeFilm(8, 6);
  REQUIRE(f.film.size() == 8u * 6u);
}

TEST_CASE("Film init: SPP is zero") {
  Film f = makeFilm();
  REQUIRE(f.SPP == 0);
}

TEST_CASE("Film init: all pixels start at zero") {
  Film f = makeFilm();
  for (const Colour& c : f.film) {
    REQUIRE(c.r == Catch::Approx(0.0f));
    REQUIRE(c.g == Catch::Approx(0.0f));
    REQUIRE(c.b == Catch::Approx(0.0f));
  }
}

// -----------------------------------------------------------------------
// splat
// -----------------------------------------------------------------------

TEST_CASE("Film splat: accumulates colour at correct pixel") {
  Film f = makeFilm();
  f.splat(1.0f, 2.0f, Colour(0.5f, 0.25f, 0.1f));
  Colour& px = f.film[2 * f.width + 1];
  REQUIRE(px.r == Catch::Approx(0.5f));
  REQUIRE(px.g == Catch::Approx(0.25f));
  REQUIRE(px.b == Catch::Approx(0.1f));
}

TEST_CASE("Film splat: multiple splats accumulate") {
  Film f = makeFilm();
  f.splat(0.0f, 0.0f, Colour(0.2f, 0.2f, 0.2f));
  f.splat(0.0f, 0.0f, Colour(0.3f, 0.3f, 0.3f));
  REQUIRE(f.film[0].r == Catch::Approx(0.5f));
}

TEST_CASE("Film splat: out-of-bounds sample is discarded") {
  Film f = makeFilm(4, 4);
  f.splat(-1.0f,  0.0f, Colour(1.0f, 1.0f, 1.0f));
  f.splat( 4.0f,  0.0f, Colour(1.0f, 1.0f, 1.0f));
  f.splat( 0.0f, -1.0f, Colour(1.0f, 1.0f, 1.0f));
  f.splat( 0.0f,  4.0f, Colour(1.0f, 1.0f, 1.0f));
  for (const Colour& c : f.film) {
    REQUIRE(c.r == Catch::Approx(0.0f));
  }
}

// -----------------------------------------------------------------------
// clear
// -----------------------------------------------------------------------

TEST_CASE("Film clear: zeros all pixels after splat") {
  Film f = makeFilm();
  f.splat(0.0f, 0.0f, Colour(1.0f, 1.0f, 1.0f));
  f.incrementSPP();
  f.clear();
  for (const Colour& c : f.film) {
    REQUIRE(c.r == Catch::Approx(0.0f));
  }
  REQUIRE(f.SPP == 0);
}

// -----------------------------------------------------------------------
// incrementSPP
// -----------------------------------------------------------------------

TEST_CASE("Film incrementSPP: increments counter") {
  Film f = makeFilm();
  f.incrementSPP();
  f.incrementSPP();
  REQUIRE(f.SPP == 2);
}

// -----------------------------------------------------------------------
// tonemap
// -----------------------------------------------------------------------

TEST_CASE("Film tonemap: SPP == 0 returns black") {
  Film f = makeFilm();
  f.splat(0.0f, 0.0f, Colour(1.0f, 1.0f, 1.0f));
  // SPP still 0 — should return black regardless of accumulated value
  unsigned char r, g, b;
  f.tonemap(0, 0, r, g, b);
  REQUIRE(r == 0);
  REQUIRE(g == 0);
  REQUIRE(b == 0);
}

TEST_CASE("Film tonemap: black pixel maps to zero output") {
  Film f = makeFilm();
  f.splat(0.0f, 0.0f, Colour(0.0f, 0.0f, 0.0f));
  f.incrementSPP();
  unsigned char r, g, b;
  f.tonemap(0, 0, r, g, b);
  REQUIRE(r == 0);
  REQUIRE(g == 0);
  REQUIRE(b == 0);
}

TEST_CASE("Film tonemap: SPP averaging — two identical splats equal one splat (bug regression)") {
  // Without the SPP divide fix, splatting twice would give 2x brightness.
  Film f1 = makeFilm();
  f1.splat(0.0f, 0.0f, Colour(0.5f, 0.5f, 0.5f));
  f1.incrementSPP();

  Film f2 = makeFilm();
  f2.splat(0.0f, 0.0f, Colour(0.5f, 0.5f, 0.5f));
  f2.splat(0.0f, 0.0f, Colour(0.5f, 0.5f, 0.5f));
  f2.incrementSPP();
  f2.incrementSPP();

  unsigned char r1, g1, b1, r2, g2, b2;
  f1.tonemap(0, 0, r1, g1, b1);
  f2.tonemap(0, 0, r2, g2, b2);
  REQUIRE(r1 == r2);
  REQUIRE(g1 == g2);
  REQUIRE(b1 == b2);
}

TEST_CASE("Film tonemap: higher exposure produces brighter output") {
  Film f = makeFilm();
  f.splat(0.0f, 0.0f, Colour(0.3f, 0.3f, 0.3f));
  f.incrementSPP();

  unsigned char r1, g1, b1, r2, g2, b2;
  f.tonemap(0, 0, r1, g1, b1, 1.0f);
  f.tonemap(0, 0, r2, g2, b2, 2.0f);
  REQUIRE(r2 > r1);
  REQUIRE(g2 > g1);
  REQUIRE(b2 > b1);
}

TEST_CASE("Film tonemap: very bright input is clamped to 255") {
  Film f = makeFilm();
  f.splat(0.0f, 0.0f, Colour(1000.0f, 1000.0f, 1000.0f));
  f.incrementSPP();
  unsigned char r, g, b;
  f.tonemap(0, 0, r, g, b);
  REQUIRE(r == 255);
  REQUIRE(g == 255);
  REQUIRE(b == 255);
}

// -----------------------------------------------------------------------
// toPixels
// -----------------------------------------------------------------------

TEST_CASE("Film toPixels: buffer size is width * height * 3") {
  Film f = makeFilm(8, 6);
  f.incrementSPP();
  auto pixels = f.toPixels();
  REQUIRE(pixels.size() == 8u * 6u * 3u);
}

TEST_CASE("Film toPixels: all black when SPP == 0") {
  Film f = makeFilm();
  f.splat(0.0f, 0.0f, Colour(1.0f, 1.0f, 1.0f));
  auto pixels = f.toPixels();
  for (auto p : pixels) REQUIRE(p == 0);
}

TEST_CASE("Film toPixels: non-zero pixel after splat and incrementSPP") {
  Film f = makeFilm();
  f.splat(0.0f, 0.0f, Colour(0.5f, 0.5f, 0.5f));
  f.incrementSPP();
  auto pixels = f.toPixels();
  REQUIRE(pixels[0] > 0);
  REQUIRE(pixels[1] > 0);
  REQUIRE(pixels[2] > 0);
}
