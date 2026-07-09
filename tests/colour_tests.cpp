#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "core.h"

TEST_CASE("Colour constructors") {
  SECTION("default constructor is black") {
    Colour c;
    REQUIRE(c.r == 0.0f);
    REQUIRE(c.g == 0.0f);
    REQUIRE(c.b == 0.0f);
  }

  SECTION("float constructor stores values correctly") {
    Colour c(0.1f, 0.5f, 0.9f);
    REQUIRE(c.r == Catch::Approx(0.1f));
    REQUIRE(c.g == Catch::Approx(0.5f));
    REQUIRE(c.b == Catch::Approx(0.9f));
  }

  SECTION("unsigned char constructor converts to 0-1 range") {
    Colour c((unsigned char)255, (unsigned char)255, (unsigned char)255);
    REQUIRE(c.r == Catch::Approx(1.0f));
    REQUIRE(c.g == Catch::Approx(1.0f));
    REQUIRE(c.b == Catch::Approx(1.0f));
  }

  SECTION("unsigned char black is 0") {
    Colour c((unsigned char)0, (unsigned char)0, (unsigned char)0);
    REQUIRE(c.r == 0.0f);
    REQUIRE(c.g == 0.0f);
    REQUIRE(c.b == 0.0f);
  }
}

TEST_CASE("Colour arithmetic") {
  SECTION("addition") {
    Colour a(0.1f, 0.2f, 0.3f);
    Colour b(0.4f, 0.5f, 0.6f);
    Colour result = a + b;
    REQUIRE(result.r == Catch::Approx(0.5f));
    REQUIRE(result.g == Catch::Approx(0.7f));
    REQUIRE(result.b == Catch::Approx(0.9f));
  }

  SECTION("subtraction") {
    Colour a(0.5f, 0.5f, 0.5f);
    Colour b(0.1f, 0.2f, 0.3f);
    Colour result = a - b;
    REQUIRE(result.r == Catch::Approx(0.4f));
    REQUIRE(result.g == Catch::Approx(0.3f));
    REQUIRE(result.b == Catch::Approx(0.2f));
  }

  SECTION("colour multiply (albedo modulation)") {
    Colour a(0.5f, 0.5f, 0.5f);
    Colour b(0.5f, 0.5f, 0.5f);
    Colour result = a * b;
    REQUIRE(result.r == Catch::Approx(0.25f));
    REQUIRE(result.g == Catch::Approx(0.25f));
    REQUIRE(result.b == Catch::Approx(0.25f));
  }

  SECTION("scalar multiply") {
    Colour c(0.5f, 0.5f, 0.5f);
    Colour result = c * 2.0f;
    REQUIRE(result.r == Catch::Approx(1.0f));
    REQUIRE(result.g == Catch::Approx(1.0f));
    REQUIRE(result.b == Catch::Approx(1.0f));
  }

  SECTION("scalar divide") {
    Colour c(0.6f, 0.4f, 0.2f);
    Colour result = c / 2.0f;
    REQUIRE(result.r == Catch::Approx(0.3f));
    REQUIRE(result.g == Catch::Approx(0.2f));
    REQUIRE(result.b == Catch::Approx(0.1f));
  }
}

TEST_CASE("Colour toRGB") {
  SECTION("white maps to 255,255,255") {
    Colour c(1.0f, 0.0f, 0.0f);
    unsigned char r, g, b;
    c.toRGB(r, g, b);
    REQUIRE(r == 255);
    REQUIRE(g == 0);
    REQUIRE(b == 0);
  }

  SECTION("black maps to 0,0,0") {
    Colour c(0.0f, 0.0f, 0.0f);
    unsigned char r, g, b;
    c.toRGB(r, g, b);
    REQUIRE(r == 0);
    REQUIRE(g == 0);
    REQUIRE(b == 0);
  }

  SECTION("clamps values above 1") {
    Colour c(2.0f, 0.0f, 0.0f);
    unsigned char r, g, b;
    c.toRGB(r, g, b);
    REQUIRE(r == 255);
  }

  SECTION("clamps values below 0") {
    Colour c(-1.0f, 0.0f, 0.0f);
    unsigned char r, g, b;
    c.toRGB(r, g, b);
    REQUIRE(r == 0);
  }
}

TEST_CASE("Colour lum") {
  SECTION("pure red returns red weight") {
    Colour c(1.0f, 0.0f, 0.0f);
    REQUIRE(c.lum() == Catch::Approx(LUM_R));
  }

  SECTION("pure green returns green weight") {
    Colour c(0.0f, 1.0f, 0.0f);
    REQUIRE(c.lum() == Catch::Approx(LUM_G));
  }

  SECTION("pure blue returns blue weight") {
    Colour c(0.0f, 0.0f, 1.0f);
    REQUIRE(c.lum() == Catch::Approx(LUM_B));
  }

  SECTION("white has luminance 1") {
    Colour c(1.0f, 1.0f, 1.0f);
    REQUIRE(c.lum() == Catch::Approx(1.0f));
  }

  SECTION("black has luminance 0") {
    Colour c(0.0f, 0.0f, 0.0f);
    REQUIRE(c.lum() == 0.0f);
  }
}
