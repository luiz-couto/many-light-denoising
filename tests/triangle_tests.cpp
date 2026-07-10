#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <vector>
#include "geometry.h"

// Feeds a fixed sequence of floats so sample() is deterministic in tests
class SequenceSampler : public Sampler {
  std::vector<float> seq;
  size_t idx = 0;
public:
  SequenceSampler(std::initializer_list<float> vals) : seq(vals) {}
  float next() override { return seq[idx++ % seq.size()]; }
};

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

// -----------------------------------------------------------------------
// Triangle::sample
// -----------------------------------------------------------------------

TEST_CASE("Triangle::sample pdf equals 1/area") {
  Triangle t = makeXYTriangle();  // area = 0.5
  MTRandom rng(42);
  float pdf;
  t.sample(&rng, pdf);
  REQUIRE(pdf == Catch::Approx(2.0f).margin(1e-4f));  // 1 / 0.5
}

TEST_CASE("Triangle::sample point lies on triangle plane") {
  Triangle t = makeXYTriangle();  // z = 0 plane
  MTRandom rng(42);
  float pdf;
  for (int i = 0; i < 500; i++) {
    Vec3 p = t.sample(&rng, pdf);
    REQUIRE(p.z == Catch::Approx(0.0f).margin(1e-4f));
  }
}

TEST_CASE("Triangle::sample point is inside triangle") {
  // XY triangle: x>=0, y>=0, x+y<=1
  Triangle t = makeXYTriangle();
  MTRandom rng(42);
  float pdf;
  for (int i = 0; i < 500; i++) {
    Vec3 p = t.sample(&rng, pdf);
    REQUIRE(p.x >= -1e-4f);
    REQUIRE(p.y >= -1e-4f);
    REQUIRE(p.x + p.y <= 1.0f + 1e-4f);
  }
}

TEST_CASE("Triangle::sample corner cases reach each vertex") {
  Triangle t = makeXYTriangle();
  // v0=(0,0,0), v1=(1,0,0), v2=(0,1,0)
  float pdf;

  SECTION("r1=0 -> v0") {
    SequenceSampler s({0.0f, 0.5f});
    Vec3 p = t.sample(&s, pdf);
    requireVecEq(p, 0.0f, 0.0f, 0.0f);
  }

  SECTION("r1=1, r2=1 -> v1") {
    SequenceSampler s({1.0f, 1.0f});
    Vec3 p = t.sample(&s, pdf);
    requireVecEq(p, 1.0f, 0.0f, 0.0f);
  }

  SECTION("r1=1, r2=0 -> v2") {
    SequenceSampler s({1.0f, 0.0f});
    Vec3 p = t.sample(&s, pdf);
    requireVecEq(p, 0.0f, 1.0f, 0.0f);
  }
}

TEST_CASE("Triangle::sample E[P] converges to centroid") {
  // For any uniform distribution on a triangle, the expected sample position
  // equals the centroid: E[P] = (v0+v1+v2)/3
  Triangle t = makeXYTriangle();
  MTRandom rng(42);
  float pdf;
  Vec3 sum(0.0f, 0.0f, 0.0f);
  const int N = 1000;
  for (int i = 0; i < N; i++)
    sum = sum + t.sample(&rng, pdf);
  Vec3 mean = sum / static_cast<float>(N);
  // Statistical margin: std dev of mean ≈ 0.008 for N=1000, so 0.05 is >6-sigma
  REQUIRE(mean.x == Catch::Approx(1.0f / 3.0f).margin(0.05f));
  REQUIRE(mean.y == Catch::Approx(1.0f / 3.0f).margin(0.05f));
  REQUIRE(mean.z == Catch::Approx(0.0f).margin(1e-4f));
}

TEST_CASE("Triangle::sample works for arbitrary 3D triangle") {
  // Non-axis-aligned triangle — verify point lies on the plane: dot(P-v0, n) = 0
  Vertex v0(Vec3(1.0f, 0.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f), 0.0f, 0.0f);
  Vertex v1(Vec3(0.0f, 1.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f), 1.0f, 0.0f);
  Vertex v2(Vec3(0.0f, 0.0f, 1.0f), Vec3(0.0f, 1.0f, 0.0f), 0.0f, 1.0f);
  Triangle t;
  t.init(v0, v1, v2, 0);

  MTRandom rng(7);
  float pdf;
  for (int i = 0; i < 200; i++) {
    Vec3 p = t.sample(&rng, pdf);
    float dist = dot(p - t.vertices[0].p, t.n);
    REQUIRE(dist == Catch::Approx(0.0f).margin(1e-4f));
  }
}
