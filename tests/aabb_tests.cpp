#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cfloat>
#include "geometry.h"

// Unit cube [-1,1]^3, area = 24
static AABB makeUnitCube() {
  AABB box;
  box.extend(Vec3(-1.0f, -1.0f, -1.0f));
  box.extend(Vec3( 1.0f,  1.0f,  1.0f));
  return box;
}

// -----------------------------------------------------------------------
// Construction / reset
// -----------------------------------------------------------------------

TEST_CASE("AABB default constructor initializes to empty state") {
  AABB box;
  REQUIRE(box.bmin.x == FLT_MAX);
  REQUIRE(box.bmin.y == FLT_MAX);
  REQUIRE(box.bmin.z == FLT_MAX);
  REQUIRE(box.bmax.x == -FLT_MAX);
  REQUIRE(box.bmax.y == -FLT_MAX);
  REQUIRE(box.bmax.z == -FLT_MAX);
}

TEST_CASE("AABB reset restores empty state after extend") {
  AABB box = makeUnitCube();
  box.reset();
  REQUIRE(box.bmin.x == FLT_MAX);
  REQUIRE(box.bmax.x == -FLT_MAX);
}

// -----------------------------------------------------------------------
// extend
// -----------------------------------------------------------------------

TEST_CASE("AABB extend with single point: bmin == bmax == point") {
  AABB box;
  box.extend(Vec3(2.0f, 3.0f, 4.0f));
  REQUIRE(box.bmin.x == Catch::Approx(2.0f));
  REQUIRE(box.bmin.y == Catch::Approx(3.0f));
  REQUIRE(box.bmin.z == Catch::Approx(4.0f));
  REQUIRE(box.bmax.x == Catch::Approx(2.0f));
  REQUIRE(box.bmax.y == Catch::Approx(3.0f));
  REQUIRE(box.bmax.z == Catch::Approx(4.0f));
}

TEST_CASE("AABB extend computes correct bmin and bmax") {
  AABB box;
  box.extend(Vec3( 1.0f,  2.0f,  3.0f));
  box.extend(Vec3(-1.0f, -2.0f, -3.0f));
  box.extend(Vec3( 0.5f,  4.0f, -1.0f));
  REQUIRE(box.bmin.x == Catch::Approx(-1.0f));
  REQUIRE(box.bmin.y == Catch::Approx(-2.0f));
  REQUIRE(box.bmin.z == Catch::Approx(-3.0f));
  REQUIRE(box.bmax.x == Catch::Approx( 1.0f));
  REQUIRE(box.bmax.y == Catch::Approx( 4.0f));
  REQUIRE(box.bmax.z == Catch::Approx( 3.0f));
}

// -----------------------------------------------------------------------
// area
// -----------------------------------------------------------------------

TEST_CASE("AABB area of unit cube [-1,1]^3 is 24") {
  AABB box = makeUnitCube();
  REQUIRE(box.area() == Catch::Approx(24.0f));
}

TEST_CASE("AABB area of rectangular box") {
  // [0,2] x [0,1] x [0,3]: 2*(2*1 + 1*3 + 2*3) = 2*11 = 22
  AABB box;
  box.extend(Vec3(0.0f, 0.0f, 0.0f));
  box.extend(Vec3(2.0f, 1.0f, 3.0f));
  REQUIRE(box.area() == Catch::Approx(22.0f));
}

TEST_CASE("AABB area of flat (degenerate) box") {
  // [0,2] x [0,0] x [0,3]: 2*(0 + 0 + 6) = 12
  AABB box;
  box.extend(Vec3(0.0f, 0.0f, 0.0f));
  box.extend(Vec3(2.0f, 0.0f, 3.0f));
  REQUIRE(box.area() == Catch::Approx(12.0f));
}

// -----------------------------------------------------------------------
// rayAABB — hits
// -----------------------------------------------------------------------

TEST_CASE("AABB rayAABB hit from front returns correct t") {
  // Ray from (0,0,-5) toward +z enters unit cube at z=-1, so t=4
  AABB box = makeUnitCube();
  Ray r(Vec3(0.0f, 0.0f, -5.0f), Vec3(0.0f, 0.0f, 1.0f));
  float t;
  REQUIRE(box.rayAABB(r, t));
  REQUIRE(t == Catch::Approx(4.0f).margin(1e-4f));
}

TEST_CASE("AABB rayAABB hit from side returns correct t") {
  // Ray from (-5,0,0) toward +x enters unit cube at x=-1, so t=4
  AABB box = makeUnitCube();
  Ray r(Vec3(-5.0f, 0.0f, 0.0f), Vec3(1.0f, 0.0f, 0.0f));
  float t;
  REQUIRE(box.rayAABB(r, t));
  REQUIRE(t == Catch::Approx(4.0f).margin(1e-4f));
}

TEST_CASE("AABB rayAABB hit with diagonal ray") {
  AABB box = makeUnitCube();
  // Ray from (0, 0, -2) toward (0, 0, 1) shifted to origin inside the cube
  // Ray from (-3, -3, -3) toward normalized (1,1,1)
  float invSqrt3 = 1.0f / sqrtf(3.0f);
  Ray r(Vec3(-3.0f, -3.0f, -3.0f), Vec3(invSqrt3, invSqrt3, invSqrt3));
  float t;
  REQUIRE(box.rayAABB(r, t));
  // Entry should be at (-1,-1,-1), distance from (-3,-3,-3) = sqrt(4+4+4) = 2*sqrt(3)
  REQUIRE(t == Catch::Approx(2.0f * sqrtf(3.0f)).margin(1e-3f));
}

TEST_CASE("AABB rayAABB ray origin inside box: hit with negative t") {
  AABB box = makeUnitCube();
  Ray r(Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f));
  float t;
  REQUIRE(box.rayAABB(r, t));
  REQUIRE(t < 0.0f);  // entry is behind origin
}

// -----------------------------------------------------------------------
// rayAABB — misses
// -----------------------------------------------------------------------

TEST_CASE("AABB rayAABB ray pointing away from box") {
  AABB box = makeUnitCube();
  Ray r(Vec3(0.0f, 0.0f, -5.0f), Vec3(0.0f, 0.0f, -1.0f));
  float t;
  REQUIRE_FALSE(box.rayAABB(r, t));
}

TEST_CASE("AABB rayAABB ray passes beside box") {
  AABB box = makeUnitCube();
  // x=2 is outside [-1,1]
  Ray r(Vec3(2.0f, 0.0f, -5.0f), Vec3(0.0f, 0.0f, 1.0f));
  float t;
  REQUIRE_FALSE(box.rayAABB(r, t));
}

TEST_CASE("AABB rayAABB ray passes above box") {
  AABB box = makeUnitCube();
  // y=5 is outside [-1,1]
  Ray r(Vec3(0.0f, 5.0f, -5.0f), Vec3(0.0f, 0.0f, 1.0f));
  float t;
  REQUIRE_FALSE(box.rayAABB(r, t));
}

// -----------------------------------------------------------------------
// rayAABB no-t overload consistent with t overload
// -----------------------------------------------------------------------

TEST_CASE("AABB rayAABB overloads agree on hits and misses") {
  AABB box = makeUnitCube();

  Ray hit(Vec3(0.0f, 0.0f, -5.0f), Vec3(0.0f, 0.0f,  1.0f));
  Ray miss(Vec3(2.0f, 0.0f, -5.0f), Vec3(0.0f, 0.0f, 1.0f));

  float t;
  REQUIRE(box.rayAABB(hit,  t) == box.rayAABB(hit));
  REQUIRE(box.rayAABB(miss, t) == box.rayAABB(miss));
}
