#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "core.h"

static void requireVecEq(const Vec3& v, float x, float y, float z) {
  REQUIRE(v.x == Catch::Approx(x).margin(1e-4f));
  REQUIRE(v.y == Catch::Approx(y).margin(1e-4f));
  REQUIRE(v.z == Catch::Approx(z).margin(1e-4f));
}

// -----------------------------------------------------------------------
// dot
// -----------------------------------------------------------------------

TEST_CASE("dot") {
  SECTION("perpendicular vectors are 0") {
    REQUIRE(dot(Vec3(1.0f, 0.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f)) == Catch::Approx(0.0f));
  }

  SECTION("parallel vectors equal product of lengths") {
    REQUIRE(dot(Vec3(2.0f, 0.0f, 0.0f), Vec3(3.0f, 0.0f, 0.0f)) == Catch::Approx(6.0f));
  }

  SECTION("dot with itself equals lengthSq") {
    Vec3 v(1.0f, 2.0f, 3.0f);
    REQUIRE(dot(v, v) == Catch::Approx(v.lengthSq()));
  }

  SECTION("commutative") {
    Vec3 a(1.0f, 2.0f, 3.0f);
    Vec3 b(4.0f, 5.0f, 6.0f);
    REQUIRE(dot(a, b) == Catch::Approx(dot(b, a)));
  }
}

// -----------------------------------------------------------------------
// cross
// -----------------------------------------------------------------------

TEST_CASE("cross") {
  SECTION("X cross Y == Z") {
    requireVecEq(cross(Vec3(1.0f, 0.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f)), 0.0f, 0.0f, 1.0f);
  }

  SECTION("Y cross Z == X") {
    requireVecEq(cross(Vec3(0.0f, 1.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f)), 1.0f, 0.0f, 0.0f);
  }

  SECTION("Z cross X == Y") {
    requireVecEq(cross(Vec3(0.0f, 0.0f, 1.0f), Vec3(1.0f, 0.0f, 0.0f)), 0.0f, 1.0f, 0.0f);
  }

  SECTION("anticommutative: cross(a,b) == -cross(b,a)") {
    Vec3 a(1.0f, 2.0f, 3.0f);
    Vec3 b(4.0f, 5.0f, 6.0f);
    Vec3 ab = cross(a, b);
    Vec3 ba = cross(b, a);
    requireVecEq(ab, -ba.x, -ba.y, -ba.z);
  }

  SECTION("parallel vectors give zero vector") {
    requireVecEq(cross(Vec3(2.0f, 0.0f, 0.0f), Vec3(4.0f, 0.0f, 0.0f)), 0.0f, 0.0f, 0.0f);
  }

  SECTION("result is perpendicular to both inputs") {
    Vec3 a(1.0f, 2.0f, 3.0f);
    Vec3 b(4.0f, 5.0f, 6.0f);
    Vec3 c = cross(a, b);
    REQUIRE(dot(c, a) == Catch::Approx(0.0f).margin(1e-4f));
    REQUIRE(dot(c, b) == Catch::Approx(0.0f).margin(1e-4f));
  }
}

// -----------------------------------------------------------------------
// minVec
// -----------------------------------------------------------------------

TEST_CASE("minVec") {
  SECTION("picks smaller component from each") {
    requireVecEq(minVec(Vec3(1.0f, 5.0f, 3.0f), Vec3(4.0f, 2.0f, 6.0f)), 1.0f, 2.0f, 3.0f);
  }

  SECTION("equal vectors return the same vector") {
    Vec3 v(2.0f, 3.0f, 4.0f);
    requireVecEq(minVec(v, v), 2.0f, 3.0f, 4.0f);
  }

  SECTION("negative values") {
    requireVecEq(minVec(Vec3(-1.0f, 0.0f, 2.0f), Vec3(1.0f, -3.0f, 1.0f)), -1.0f, -3.0f, 1.0f);
  }
}

// -----------------------------------------------------------------------
// maxVec
// -----------------------------------------------------------------------

TEST_CASE("maxVec") {
  SECTION("picks larger component from each") {
    requireVecEq(maxVec(Vec3(1.0f, 5.0f, 3.0f), Vec3(4.0f, 2.0f, 6.0f)), 4.0f, 5.0f, 6.0f);
  }

  SECTION("equal vectors return the same vector") {
    Vec3 v(2.0f, 3.0f, 4.0f);
    requireVecEq(maxVec(v, v), 2.0f, 3.0f, 4.0f);
  }

  SECTION("negative values") {
    requireVecEq(maxVec(Vec3(-1.0f, 0.0f, 2.0f), Vec3(1.0f, -3.0f, 1.0f)), 1.0f, 0.0f, 2.0f);
  }
}
