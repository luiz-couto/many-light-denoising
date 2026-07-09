#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "core.h"

static void requireVecEq(const Vec3& v, float x, float y, float z) {
  REQUIRE(v.x == Catch::Approx(x).margin(1e-4f));
  REQUIRE(v.y == Catch::Approx(y).margin(1e-4f));
  REQUIRE(v.z == Catch::Approx(z).margin(1e-4f));
}

// -----------------------------------------------------------------------
// sphericalToWorld
// -----------------------------------------------------------------------

TEST_CASE("SphericalCoordinates::sphericalToWorld known directions") {
  SECTION("theta=0 is north pole (0,0,1)") {
    requireVecEq(SphericalCoordinates::sphericalToWorld(0.0f, 0.0f), 0.0f, 0.0f, 1.0f);
  }

  SECTION("theta=PI is south pole (0,0,-1)") {
    requireVecEq(SphericalCoordinates::sphericalToWorld(PI, 0.0f), 0.0f, 0.0f, -1.0f);
  }

  SECTION("theta=PI/2, phi=0 points along +X") {
    requireVecEq(SphericalCoordinates::sphericalToWorld(PI / 2.0f, 0.0f), 1.0f, 0.0f, 0.0f);
  }

  SECTION("theta=PI/2, phi=PI/2 points along +Y") {
    requireVecEq(SphericalCoordinates::sphericalToWorld(PI / 2.0f, PI / 2.0f), 0.0f, 1.0f, 0.0f);
  }

  SECTION("theta=PI/2, phi=PI points along -X") {
    requireVecEq(SphericalCoordinates::sphericalToWorld(PI / 2.0f, PI), -1.0f, 0.0f, 0.0f);
  }
}

TEST_CASE("SphericalCoordinates::sphericalToWorld result is unit length") {
  REQUIRE(SphericalCoordinates::sphericalToWorld(0.5f, 1.0f).length() == Catch::Approx(1.0f).margin(1e-4f));
  REQUIRE(SphericalCoordinates::sphericalToWorld(PI / 3.0f, PI / 4.0f).length() == Catch::Approx(1.0f).margin(1e-4f));
}

// -----------------------------------------------------------------------
// sphericalTheta
// -----------------------------------------------------------------------

TEST_CASE("SphericalCoordinates::sphericalTheta known angles") {
  SECTION("+Z gives theta=0") {
    REQUIRE(SphericalCoordinates::sphericalTheta(Vec3(0.0f, 0.0f, 1.0f)) == Catch::Approx(0.0f).margin(1e-4f));
  }

  SECTION("-Z gives theta=PI") {
    REQUIRE(SphericalCoordinates::sphericalTheta(Vec3(0.0f, 0.0f, -1.0f)) == Catch::Approx(PI).margin(1e-4f));
  }

  SECTION("+X gives theta=PI/2") {
    REQUIRE(SphericalCoordinates::sphericalTheta(Vec3(1.0f, 0.0f, 0.0f)) == Catch::Approx(PI / 2.0f).margin(1e-4f));
  }

  SECTION("+Y gives theta=PI/2") {
    REQUIRE(SphericalCoordinates::sphericalTheta(Vec3(0.0f, 1.0f, 0.0f)) == Catch::Approx(PI / 2.0f).margin(1e-4f));
  }
}

TEST_CASE("SphericalCoordinates::sphericalTheta clamps slightly out-of-range z") {
  // Simulates floating point drift past ±1 after normalization
  REQUIRE_NOTHROW(SphericalCoordinates::sphericalTheta(Vec3(0.0f, 0.0f, 1.0001f)));
  REQUIRE_NOTHROW(SphericalCoordinates::sphericalTheta(Vec3(0.0f, 0.0f, -1.0001f)));

  // Should clamp to 0 and PI respectively, not NaN
  float t1 = SphericalCoordinates::sphericalTheta(Vec3(0.0f, 0.0f, 1.0001f));
  float t2 = SphericalCoordinates::sphericalTheta(Vec3(0.0f, 0.0f, -1.0001f));
  REQUIRE(std::isfinite(t1));
  REQUIRE(std::isfinite(t2));
}

// -----------------------------------------------------------------------
// sphericalPhi
// -----------------------------------------------------------------------

TEST_CASE("SphericalCoordinates::sphericalPhi known angles") {
  SECTION("+X gives phi=0") {
    REQUIRE(SphericalCoordinates::sphericalPhi(Vec3(1.0f, 0.0f, 0.0f)) == Catch::Approx(0.0f).margin(1e-4f));
  }

  SECTION("+Y gives phi=PI/2") {
    REQUIRE(SphericalCoordinates::sphericalPhi(Vec3(0.0f, 1.0f, 0.0f)) == Catch::Approx(PI / 2.0f).margin(1e-4f));
  }

  SECTION("-X gives phi=PI") {
    REQUIRE(SphericalCoordinates::sphericalPhi(Vec3(-1.0f, 0.0f, 0.0f)) == Catch::Approx(PI).margin(1e-4f));
  }

  SECTION("-Y gives phi=3*PI/2 (negative atan2 mapped to [0,2PI])") {
    REQUIRE(SphericalCoordinates::sphericalPhi(Vec3(0.0f, -1.0f, 0.0f)) == Catch::Approx(3.0f * PI / 2.0f).margin(1e-4f));
  }
}

TEST_CASE("SphericalCoordinates::sphericalPhi result is in [0, 2*PI]") {
  Vec3 dirs[] = {
    Vec3(1.0f, 0.5f, 0.0f),
    Vec3(-1.0f, 0.5f, 0.0f),
    Vec3(0.3f, -0.9f, 0.0f),
    Vec3(-0.5f, -0.5f, 0.0f),
  };
  for (auto& d : dirs) {
    float phi = SphericalCoordinates::sphericalPhi(d.normalize());
    REQUIRE(phi >= 0.0f);
    REQUIRE(phi <= 2.0f * PI + 1e-4f);
  }
}

// -----------------------------------------------------------------------
// Roundtrip
// -----------------------------------------------------------------------

TEST_CASE("SphericalCoordinates roundtrip") {
  Vec3 dirs[] = {
    Vec3(1.0f, 0.0f, 0.0f),
    Vec3(0.0f, 1.0f, 0.0f),
    Vec3(0.0f, 0.0f, 1.0f),
    Vec3(0.577f, 0.577f, 0.577f),
    Vec3(-0.5f, 0.3f, 0.8f),
  };
  for (auto& d : dirs) {
    Vec3 v = d.normalize();
    float theta = SphericalCoordinates::sphericalTheta(v);
    float phi   = SphericalCoordinates::sphericalPhi(v);
    Vec3 result = SphericalCoordinates::sphericalToWorld(theta, phi);
    requireVecEq(result, v.x, v.y, v.z);
  }
}
