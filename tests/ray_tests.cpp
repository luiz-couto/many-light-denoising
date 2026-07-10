#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
#include "ray.h"

static void requireVecEq(const Vec3& v, float x, float y, float z) {
  REQUIRE(v.x == Catch::Approx(x).margin(1e-4f));
  REQUIRE(v.y == Catch::Approx(y).margin(1e-4f));
  REQUIRE(v.z == Catch::Approx(z).margin(1e-4f));
}

// -----------------------------------------------------------------------
// Construction
// -----------------------------------------------------------------------

TEST_CASE("Ray constructor sets origin and direction") {
  Ray r(Vec3(1.0f, 2.0f, 3.0f), Vec3(0.0f, 0.0f, 1.0f));
  requireVecEq(r.o,   1.0f, 2.0f, 3.0f);
  requireVecEq(r.dir, 0.0f, 0.0f, 1.0f);
}

TEST_CASE("Ray constructor precomputes invDir") {
  Ray r(Vec3(0.0f, 0.0f, 0.0f), Vec3(2.0f, 4.0f, 0.5f));
  requireVecEq(r.invDir, 0.5f, 0.25f, 2.0f);
}

TEST_CASE("Ray init updates origin, direction and invDir") {
  Ray r;
  r.init(Vec3(1.0f, 0.0f, 0.0f), Vec3(2.0f, 1.0f, 4.0f));
  requireVecEq(r.o,      1.0f, 0.0f, 0.0f);
  requireVecEq(r.dir,    2.0f, 1.0f, 4.0f);
  requireVecEq(r.invDir, 0.5f, 1.0f, 0.25f);
}

// -----------------------------------------------------------------------
// at(t)
// -----------------------------------------------------------------------

TEST_CASE("Ray::at") {
  SECTION("t=0 returns origin") {
    Ray r(Vec3(1.0f, 2.0f, 3.0f), Vec3(0.0f, 0.0f, 1.0f));
    requireVecEq(r.at(0.0f), 1.0f, 2.0f, 3.0f);
  }

  SECTION("t=1 returns origin + direction") {
    Ray r(Vec3(1.0f, 2.0f, 3.0f), Vec3(1.0f, 0.0f, 0.0f));
    requireVecEq(r.at(1.0f), 2.0f, 2.0f, 3.0f);
  }

  SECTION("arbitrary t") {
    Ray r(Vec3(0.0f, 0.0f, 0.0f), Vec3(1.0f, 2.0f, 3.0f));
    requireVecEq(r.at(2.0f), 2.0f, 4.0f, 6.0f);
  }

  SECTION("negative t goes behind origin") {
    Ray r(Vec3(0.0f, 0.0f, 0.0f), Vec3(1.0f, 0.0f, 0.0f));
    requireVecEq(r.at(-1.0f), -1.0f, 0.0f, 0.0f);
  }
}

// -----------------------------------------------------------------------
// invDir — infinity for zero direction components
// -----------------------------------------------------------------------

TEST_CASE("Ray invDir is infinity when direction component is zero") {
  Ray r(Vec3(0.0f, 0.0f, 0.0f), Vec3(1.0f, 0.0f, 0.0f));
  REQUIRE(std::isinf(r.invDir.y));
  REQUIRE(std::isinf(r.invDir.z));
  REQUIRE(r.invDir.x == Catch::Approx(1.0f));
}

// -----------------------------------------------------------------------
// IntersectionData defaults
// -----------------------------------------------------------------------

TEST_CASE("IntersectionData default initializes safely") {
  IntersectionData d;
  REQUIRE(d.t  == FLT_MAX);
  REQUIRE(d.ID == UINT_MAX);
  REQUIRE(d.alpha == 0.0f);
  REQUIRE(d.beta  == 0.0f);
  REQUIRE(d.gamma == 0.0f);
}
