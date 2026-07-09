#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <stdexcept>
#include "core.h"

static void requireUnit(const Vec3& v) {
  float len = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
  REQUIRE(len == Catch::Approx(1.0f).margin(1e-4f));
}

static void requireOrthogonal(const Vec3& a, const Vec3& b) {
  REQUIRE(dot(a, b) == Catch::Approx(0.0f).margin(1e-4f));
}

static void requireOrthonormal(const Frame& f) {
  requireUnit(f.u);
  requireUnit(f.v);
  requireUnit(f.w);
  requireOrthogonal(f.u, f.v);
  requireOrthogonal(f.u, f.w);
  requireOrthogonal(f.v, f.w);
}

static void requireVecEq(const Vec3& a, float x, float y, float z) {
  REQUIRE(a.x == Catch::Approx(x).margin(1e-4f));
  REQUIRE(a.y == Catch::Approx(y).margin(1e-4f));
  REQUIRE(a.z == Catch::Approx(z).margin(1e-4f));
}

// -----------------------------------------------------------------------
// fromVector — orthonormality
// -----------------------------------------------------------------------

TEST_CASE("Frame::fromVector produces orthonormal basis") {
  SECTION("Y up") {
    Frame f;
    f.fromVector(Vec3(0.0f, 1.0f, 0.0f));
    requireOrthonormal(f);
  }

  SECTION("X normal") {
    Frame f;
    f.fromVector(Vec3(1.0f, 0.0f, 0.0f));
    requireOrthonormal(f);
  }

  SECTION("Z normal") {
    Frame f;
    f.fromVector(Vec3(0.0f, 0.0f, 1.0f));
    requireOrthonormal(f);
  }

  SECTION("-Z normal") {
    Frame f;
    f.fromVector(Vec3(0.0f, 0.0f, -1.0f));
    requireOrthonormal(f);
  }

  SECTION("arbitrary normal") {
    Frame f;
    f.fromVector(Vec3(0.3f, 0.7f, 0.2f));
    requireOrthonormal(f);
  }

  SECTION("X-dominant normal (exercises first branch)") {
    Frame f;
    f.fromVector(Vec3(0.8f, 0.1f, 0.1f));
    requireOrthonormal(f);
  }

  SECTION("Y-dominant normal (exercises second branch)") {
    Frame f;
    f.fromVector(Vec3(0.1f, 0.8f, 0.1f));
    requireOrthonormal(f);
  }
}

TEST_CASE("Frame::fromVector w equals normalized input") {
  Vec3 n(1.0f, 2.0f, 3.0f);
  Vec3 nNorm = n.normalize();
  Frame f;
  f.fromVector(n);
  requireVecEq(f.w, nNorm.x, nNorm.y, nNorm.z);
}

// -----------------------------------------------------------------------
// fromVectorTangent — orthonormality + Gram-Schmidt
// -----------------------------------------------------------------------

TEST_CASE("Frame::fromVectorTangent produces orthonormal basis") {
  SECTION("perpendicular tangent") {
    Frame f;
    f.fromVectorTangent(Vec3(0.0f, 1.0f, 0.0f), Vec3(1.0f, 0.0f, 0.0f));
    requireOrthonormal(f);
  }

  SECTION("arbitrary normal and tangent") {
    Frame f;
    f.fromVectorTangent(Vec3(0.0f, 1.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f));
    requireOrthonormal(f);
  }
}

TEST_CASE("Frame::fromVectorTangent reorthogonalizes non-perpendicular tangent") {
  // tangent has a small component along the normal — simulates raw mesh tangent
  Vec3 n(0.0f, 1.0f, 0.0f);
  Vec3 t(1.0f, 0.3f, 0.0f);  // not perpendicular to n
  Frame f;
  f.fromVectorTangent(n, t);
  requireOrthonormal(f);
  // specifically: u must be perpendicular to w after Gram-Schmidt
  requireOrthogonal(f.u, f.w);
}

// -----------------------------------------------------------------------
// toLocal — basis vectors map to standard axes
// -----------------------------------------------------------------------

TEST_CASE("Frame::toLocal maps basis vectors to standard axes") {
  Frame f;
  f.fromVector(Vec3(0.0f, 1.0f, 0.0f));

  SECTION("w maps to (0,0,1)") {
    requireVecEq(f.toLocal(f.w), 0.0f, 0.0f, 1.0f);
  }

  SECTION("u maps to (1,0,0)") {
    requireVecEq(f.toLocal(f.u), 1.0f, 0.0f, 0.0f);
  }

  SECTION("v maps to (0,1,0)") {
    requireVecEq(f.toLocal(f.v), 0.0f, 1.0f, 0.0f);
  }
}

// -----------------------------------------------------------------------
// toWorld — standard axes map to basis vectors
// -----------------------------------------------------------------------

TEST_CASE("Frame::toWorld maps standard axes to basis vectors") {
  Frame f;
  f.fromVector(Vec3(0.3f, 0.7f, 0.2f));

  SECTION("(0,0,1) maps to w") {
    requireVecEq(f.toWorld(Vec3(0.0f, 0.0f, 1.0f)), f.w.x, f.w.y, f.w.z);
  }

  SECTION("(1,0,0) maps to u") {
    requireVecEq(f.toWorld(Vec3(1.0f, 0.0f, 0.0f)), f.u.x, f.u.y, f.u.z);
  }

  SECTION("(0,1,0) maps to v") {
    requireVecEq(f.toWorld(Vec3(0.0f, 1.0f, 0.0f)), f.v.x, f.v.y, f.v.z);
  }
}

// -----------------------------------------------------------------------
// Roundtrip
// -----------------------------------------------------------------------

TEST_CASE("Frame toLocal/toWorld roundtrip") {
  Frame f;
  f.fromVector(Vec3(0.5f, 0.5f, 0.7071f));

  SECTION("toWorld(toLocal(v)) == v") {
    Vec3 v(1.0f, 2.0f, 3.0f);
    Vec3 result = f.toWorld(f.toLocal(v));
    requireVecEq(result, v.x, v.y, v.z);
  }

  SECTION("toLocal(toWorld(v)) == v") {
    Vec3 v(0.5f, -0.3f, 0.8f);
    Vec3 result = f.toLocal(f.toWorld(v));
    requireVecEq(result, v.x, v.y, v.z);
  }
}
