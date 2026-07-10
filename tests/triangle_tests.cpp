#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "geometry.h"

static void requireVecEq(const Vec3& v, float x, float y, float z) {
  REQUIRE(v.x == Catch::Approx(x).margin(1e-4f));
  REQUIRE(v.y == Catch::Approx(y).margin(1e-4f));
  REQUIRE(v.z == Catch::Approx(z).margin(1e-4f));
}

// Builds a flat XY-plane triangle: v0=(0,0,0), v1=(1,0,0), v2=(0,1,0)
// Normal = (0,0,1), area = 0.5
static Triangle makeXYTriangle() {
  Vertex v0(Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f), 0.0f, 0.0f);
  Vertex v1(Vec3(1.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f), 1.0f, 0.0f);
  Vertex v2(Vec3(0.0f, 1.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f), 0.0f, 1.0f);
  Triangle t;
  t.init(v0, v1, v2, 0);
  return t;
}

// -----------------------------------------------------------------------
// Initialization
// -----------------------------------------------------------------------

TEST_CASE("Triangle::init stores vertices and material index") {
  Triangle t = makeXYTriangle();
  requireVecEq(t.vertices[0].p, 0.0f, 0.0f, 0.0f);
  requireVecEq(t.vertices[1].p, 1.0f, 0.0f, 0.0f);
  requireVecEq(t.vertices[2].p, 0.0f, 1.0f, 0.0f);
  REQUIRE(t.materialIndex == 0);
}

TEST_CASE("Triangle::init computes standard edges") {
  Triangle t = makeXYTriangle();
  requireVecEq(t.e1, 1.0f, 0.0f, 0.0f);  // v1 - v0
  requireVecEq(t.e2, 0.0f, 1.0f, 0.0f);  // v2 - v0
}

TEST_CASE("Triangle::init computes correct normal") {
  Triangle t = makeXYTriangle();
  requireVecEq(t.n, 0.0f, 0.0f, 1.0f);
  REQUIRE(t.n.length() == Catch::Approx(1.0f).margin(1e-4f));
}

TEST_CASE("Triangle::init normal is perpendicular to both edges") {
  Triangle t = makeXYTriangle();
  REQUIRE(dot(t.n, t.e1) == Catch::Approx(0.0f).margin(1e-4f));
  REQUIRE(dot(t.n, t.e2) == Catch::Approx(0.0f).margin(1e-4f));
}

TEST_CASE("Triangle::init computes correct area") {
  Triangle t = makeXYTriangle();
  REQUIRE(t.area == Catch::Approx(0.5f).margin(1e-4f));
}

TEST_CASE("Triangle::init computes correct centroid") {
  Triangle t = makeXYTriangle();
  requireVecEq(t.centroid, 1.0f / 3.0f, 1.0f / 3.0f, 0.0f);
}

// -----------------------------------------------------------------------
// rayIntersectMT — hits
// -----------------------------------------------------------------------

TEST_CASE("Triangle::rayIntersectMT hits centre of triangle") {
  Triangle t = makeXYTriangle();
  Ray r(Vec3(1.0f / 3.0f, 1.0f / 3.0f, 1.0f), Vec3(0.0f, 0.0f, -1.0f));
  float tVal, u, v;
  REQUIRE(t.rayIntersectMT(r, tVal, u, v));
  REQUIRE(tVal == Catch::Approx(1.0f).margin(1e-4f));
}

TEST_CASE("Triangle::rayIntersectMT t value gives correct hit point") {
  Triangle t = makeXYTriangle();
  Vec3 hitPoint(0.3f, 0.3f, 0.0f);
  Ray r(Vec3(0.3f, 0.3f, 2.0f), Vec3(0.0f, 0.0f, -1.0f));
  float tVal, u, v;
  REQUIRE(t.rayIntersectMT(r, tVal, u, v));
  requireVecEq(r.at(tVal), hitPoint.x, hitPoint.y, hitPoint.z);
}

TEST_CASE("Triangle::rayIntersectMT returns correct barycentric coords") {
  Triangle t = makeXYTriangle();
  // Hit at P=(0.3, 0.4, 0): u=0.3 (weight v1), v=0.4 (weight v2)
  Ray r(Vec3(0.3f, 0.4f, 1.0f), Vec3(0.0f, 0.0f, -1.0f));
  float tVal, u, v;
  REQUIRE(t.rayIntersectMT(r, tVal, u, v));
  REQUIRE(u == Catch::Approx(0.3f).margin(1e-4f));
  REQUIRE(v == Catch::Approx(0.4f).margin(1e-4f));
}

TEST_CASE("Triangle::rayIntersectMT hit near v0") {
  Triangle t = makeXYTriangle();
  Ray r(Vec3(0.05f, 0.05f, 1.0f), Vec3(0.0f, 0.0f, -1.0f));
  float tVal, u, v;
  REQUIRE(t.rayIntersectMT(r, tVal, u, v));
  REQUIRE(u == Catch::Approx(0.05f).margin(1e-4f));
  REQUIRE(v == Catch::Approx(0.05f).margin(1e-4f));
}

TEST_CASE("Triangle::rayIntersectMT hit near v1") {
  Triangle t = makeXYTriangle();
  Ray r(Vec3(0.9f, 0.05f, 1.0f), Vec3(0.0f, 0.0f, -1.0f));
  float tVal, u, v;
  REQUIRE(t.rayIntersectMT(r, tVal, u, v));
  REQUIRE(u == Catch::Approx(0.9f).margin(1e-4f));
}

TEST_CASE("Triangle::rayIntersectMT hit near v2 - was the MT bug") {
  // This is the exact case that failed in the old buggy MT implementation:
  // u=0.1 (near v1), v=0.8 (near v2). Old code returned gamma=-0.1 → miss.
  Triangle t = makeXYTriangle();
  Ray r(Vec3(0.1f, 0.8f, 1.0f), Vec3(0.0f, 0.0f, -1.0f));
  float tVal, u, v;
  REQUIRE(t.rayIntersectMT(r, tVal, u, v));
  REQUIRE(u == Catch::Approx(0.1f).margin(1e-4f));
  REQUIRE(v == Catch::Approx(0.8f).margin(1e-4f));
}

TEST_CASE("Triangle::rayIntersectMT hit from behind (back face)") {
  Triangle t = makeXYTriangle();
  Ray r(Vec3(0.25f, 0.25f, -1.0f), Vec3(0.0f, 0.0f, 1.0f));
  float tVal, u, v;
  REQUIRE(t.rayIntersectMT(r, tVal, u, v));
  REQUIRE(tVal == Catch::Approx(1.0f).margin(1e-4f));
}

// -----------------------------------------------------------------------
// rayIntersectMT — misses
// -----------------------------------------------------------------------

TEST_CASE("Triangle::rayIntersectMT misses outside all three edges") {
  Triangle t = makeXYTriangle();
  float tVal, u, v;

  SECTION("outside v1-v2 edge (u+v > 1)") {
    Ray r(Vec3(0.6f, 0.6f, 1.0f), Vec3(0.0f, 0.0f, -1.0f));
    REQUIRE_FALSE(t.rayIntersectMT(r, tVal, u, v));
  }

  SECTION("outside v0-v2 edge (u < 0)") {
    Ray r(Vec3(-0.1f, 0.5f, 1.0f), Vec3(0.0f, 0.0f, -1.0f));
    REQUIRE_FALSE(t.rayIntersectMT(r, tVal, u, v));
  }

  SECTION("outside v0-v1 edge (v < 0)") {
    Ray r(Vec3(0.5f, -0.1f, 1.0f), Vec3(0.0f, 0.0f, -1.0f));
    REQUIRE_FALSE(t.rayIntersectMT(r, tVal, u, v));
  }
}

TEST_CASE("Triangle::rayIntersectMT misses parallel ray") {
  Triangle t = makeXYTriangle();
  Ray r(Vec3(0.25f, 0.25f, 1.0f), Vec3(1.0f, 0.0f, 0.0f));
  float tVal, u, v;
  REQUIRE_FALSE(t.rayIntersectMT(r, tVal, u, v));
}

TEST_CASE("Triangle::rayIntersectMT misses ray pointing away") {
  Triangle t = makeXYTriangle();
  Ray r(Vec3(0.25f, 0.25f, 1.0f), Vec3(0.0f, 0.0f, 1.0f));
  float tVal, u, v;
  REQUIRE_FALSE(t.rayIntersectMT(r, tVal, u, v));
}

// -----------------------------------------------------------------------
// interpolateAttributes
// -----------------------------------------------------------------------

TEST_CASE("Triangle::interpolateAttributes at each vertex returns that vertex's data") {
  Vertex v0(Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f), 0.0f, 0.0f);
  Vertex v1(Vec3(1.0f, 0.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f), 1.0f, 0.0f);
  Vertex v2(Vec3(0.0f, 1.0f, 0.0f), Vec3(1.0f, 0.0f, 0.0f), 0.0f, 1.0f);
  Triangle t;
  t.init(v0, v1, v2, 0);

  Vec3 n; float iu, iv;

  SECTION("at v0: alpha=1") {
    t.interpolateAttributes(1.0f, 0.0f, 0.0f, n, iu, iv);
    requireVecEq(n, 0.0f, 0.0f, 1.0f);
    REQUIRE(iu == Catch::Approx(0.0f)); REQUIRE(iv == Catch::Approx(0.0f));
  }

  SECTION("at v1: beta=1") {
    t.interpolateAttributes(0.0f, 1.0f, 0.0f, n, iu, iv);
    requireVecEq(n, 0.0f, 1.0f, 0.0f);
    REQUIRE(iu == Catch::Approx(1.0f)); REQUIRE(iv == Catch::Approx(0.0f));
  }

  SECTION("at v2: gamma=1") {
    t.interpolateAttributes(0.0f, 0.0f, 1.0f, n, iu, iv);
    requireVecEq(n, 1.0f, 0.0f, 0.0f);
    REQUIRE(iu == Catch::Approx(0.0f)); REQUIRE(iv == Catch::Approx(1.0f));
  }
}

TEST_CASE("Triangle::interpolateAttributes produces unit-length normal") {
  Triangle t = makeXYTriangle();
  Vec3 n; float iu, iv;
  t.interpolateAttributes(0.3f, 0.4f, 0.3f, n, iu, iv);
  REQUIRE(n.length() == Catch::Approx(1.0f).margin(1e-4f));
}

TEST_CASE("Triangle::interpolateAttributes barycentric coords from MT roundtrip") {
  // u,v from MT → alpha=1-u-v, beta=u, gamma=v → hit point from interpolation
  Triangle t = makeXYTriangle();
  Ray r(Vec3(0.3f, 0.4f, 1.0f), Vec3(0.0f, 0.0f, -1.0f));
  float tVal, u, v;
  t.rayIntersectMT(r, tVal, u, v);

  Vec3 n; float iu, iv;
  t.interpolateAttributes(1.0f - u - v, u, v, n, iu, iv);
  REQUIRE(iu == Catch::Approx(0.3f).margin(1e-4f));
  REQUIRE(iv == Catch::Approx(0.4f).margin(1e-4f));
}
