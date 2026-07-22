#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <algorithm>
#include <cfloat>
#include "bvh.h"

// Flat triangle helper — normal always (0,0,1)
static Triangle makeFlatTri(Vec3 v0, Vec3 v1, Vec3 v2) {
  Vertex vt0(v0, Vec3(0,0,1), 0, 0);
  Vertex vt1(v1, Vec3(0,0,1), 1, 0);
  Vertex vt2(v2, Vec3(0,0,1), 0, 1);
  Triangle t;
  t.init(vt0, vt1, vt2, 0);
  return t;
}

// 5 triangles at z=0 + 5 triangles at z=-5, each group spanning x=[0,5].
// Used to test closest-hit selection across two BVH subtrees.
static std::vector<Triangle> makeTwoLayerScene() {
  std::vector<Triangle> tris(10);
  for (int i = 0; i < 5; i++) {
    float x = (float)i;
    tris[i]     = makeFlatTri(Vec3(x, 0, 0),  Vec3(x+1, 0, 0),  Vec3(x, 1, 0));
    tris[i + 5] = makeFlatTri(Vec3(x, 0, -5), Vec3(x+1, 0, -5), Vec3(x, 1, -5));
  }
  return tris;
}

// Builds N triangles spread evenly along the X axis.
// Each triangle is a unit right-angle in the XY plane so centroids differ in X.
static std::vector<Triangle> makeTriangleRow(int n) {
  std::vector<Triangle> tris(n);
  for (int i = 0; i < n; i++) {
    float x = (float)i;
    Vertex v0(Vec3(x,       0.0f, 0.0f), Vec3(0,0,1), 0.0f, 0.0f);
    Vertex v1(Vec3(x + 1.0f, 0.0f, 0.0f), Vec3(0,0,1), 1.0f, 0.0f);
    Vertex v2(Vec3(x,       1.0f, 0.0f), Vec3(0,0,1), 0.0f, 1.0f);
    tris[i].init(v0, v1, v2, 0);
  }
  return tris;
}

// -----------------------------------------------------------------------
// build — basic structure
// -----------------------------------------------------------------------

TEST_CASE("BVH build single triangle produces one leaf node") {
  auto tris = makeTriangleRow(1);
  BVH bvh;
  bvh.build(tris.data(), (int)tris.size());

  REQUIRE(bvh.nodes.size() == 1);
  REQUIRE(bvh.nodes[0].primCount == 1);
  REQUIRE(bvh.nodes[0].leftChild == -1);
}

TEST_CASE("BVH build with MAXLEAF_TRIANGLES triangles produces one leaf") {
  auto tris = makeTriangleRow(MAXLEAF_TRIANGLES);
  BVH bvh;
  bvh.build(tris.data(), (int)tris.size());

  REQUIRE(bvh.nodes.size() == 1);
  REQUIRE(bvh.nodes[0].primCount == MAXLEAF_TRIANGLES);
  REQUIRE(bvh.nodes[0].leftChild == -1);
}

TEST_CASE("BVH build with many triangles produces multiple nodes") {
  auto tris = makeTriangleRow(32);
  BVH bvh;
  bvh.build(tris.data(), (int)tris.size());

  REQUIRE(bvh.nodes.size() > 1);
}

// -----------------------------------------------------------------------
// primIndices
// -----------------------------------------------------------------------

TEST_CASE("BVH primIndices is a complete permutation of [0, N)") {
  const int N = 16;
  auto tris = makeTriangleRow(N);
  BVH bvh;
  bvh.build(tris.data(), N);

  REQUIRE((int)bvh.primIndices.size() == N);

  std::vector<int> sorted = bvh.primIndices;
  std::sort(sorted.begin(), sorted.end());
  for (int i = 0; i < N; i++)
    REQUIRE(sorted[i] == i);
}

// -----------------------------------------------------------------------
// root bounds
// -----------------------------------------------------------------------

TEST_CASE("BVH root node bounds contain all triangle vertices") {
  const int N = 16;
  auto tris = makeTriangleRow(N);
  BVH bvh;
  bvh.build(tris.data(), N);

  const AABB& root = bvh.nodes[0].bounds;
  for (int i = 0; i < N; i++) {
    for (int v = 0; v < 3; v++) {
      Vec3 p = tris[i].vertices[v].p;
      REQUIRE(p.x >= root.bmin.x - 1e-4f);
      REQUIRE(p.y >= root.bmin.y - 1e-4f);
      REQUIRE(p.z >= root.bmin.z - 1e-4f);
      REQUIRE(p.x <= root.bmax.x + 1e-4f);
      REQUIRE(p.y <= root.bmax.y + 1e-4f);
      REQUIRE(p.z <= root.bmax.z + 1e-4f);
    }
  }
}

// -----------------------------------------------------------------------
// tree consistency
// -----------------------------------------------------------------------

TEST_CASE("BVH leaf primCounts sum to total triangle count") {
  const int N = 32;
  auto tris = makeTriangleRow(N);
  BVH bvh;
  bvh.build(tris.data(), N);

  int total = 0;
  for (const BVHNode& node : bvh.nodes)
    if (node.primCount > 0)
      total += node.primCount;

  REQUIRE(total == N);
}

TEST_CASE("BVH interior nodes have valid child indices") {
  auto tris = makeTriangleRow(32);
  BVH bvh;
  bvh.build(tris.data(), (int)tris.size());

  int nodeCount = (int)bvh.nodes.size();
  for (const BVHNode& node : bvh.nodes) {
    if (node.primCount == 0) { // interior
      REQUIRE(node.leftChild >= 0);
      REQUIRE(node.leftChild < nodeCount);
      REQUIRE(node.leftChild + 1 < nodeCount);
    }
  }
}

TEST_CASE("BVH leaf node bounds contain their assigned triangles") {
  const int N = 32;
  auto tris = makeTriangleRow(N);
  BVH bvh;
  bvh.build(tris.data(), N);

  for (const BVHNode& node : bvh.nodes) {
    if (node.primCount == 0) continue;
    for (int i = node.firstPrim; i < node.firstPrim + node.primCount; i++) {
      const Triangle& tri = tris[bvh.primIndices[i]];
      for (int v = 0; v < 3; v++) {
        Vec3 p = tri.vertices[v].p;
        REQUIRE(p.x >= node.bounds.bmin.x - 1e-4f);
        REQUIRE(p.y >= node.bounds.bmin.y - 1e-4f);
        REQUIRE(p.z >= node.bounds.bmin.z - 1e-4f);
        REQUIRE(p.x <= node.bounds.bmax.x + 1e-4f);
        REQUIRE(p.y <= node.bounds.bmax.y + 1e-4f);
        REQUIRE(p.z <= node.bounds.bmax.z + 1e-4f);
      }
    }
  }
}

TEST_CASE("BVH no leaf node references out-of-bounds primIndices") {
  const int N = 32;
  auto tris = makeTriangleRow(N);
  BVH bvh;
  bvh.build(tris.data(), N);

  for (const BVHNode& node : bvh.nodes) {
    if (node.primCount == 0) continue;
    REQUIRE(node.firstPrim >= 0);
    REQUIRE(node.firstPrim + node.primCount <= N);
  }
}

// -----------------------------------------------------------------------
// SAH leaf decision (bug fix from old path tracer)
// -----------------------------------------------------------------------

TEST_CASE("BVH SAH prefers leaf over split for large overlapping triangles") {
  // 6 large triangles each spanning x=[0,6] with slightly different centroids.
  // Any split produces child AABBs equal in area to the parent (ratio=1), so:
  //   split cost = BOUNDS_COST(1) + (1*3 + 1*3)*C_ISECT_COST(1) = 7
  //   leaf cost  = 6 * C_ISECT_COST = 6
  // split cost > leaf cost → correct SAH makes a single leaf.
  // The old path tracer bug would split anyway.
  const int N = 6;
  std::vector<Triangle> tris(N);
  for (int i = 0; i < N; i++) {
    Vertex v0(Vec3(0.0f,      0.0f, 0.0f), Vec3(0,0,1), 0, 0);
    Vertex v1(Vec3(6.0f,      0.0f, 0.0f), Vec3(0,0,1), 1, 0);
    Vertex v2(Vec3(i * 0.1f,  1.0f, 0.0f), Vec3(0,0,1), 0, 1);
    tris[i].init(v0, v1, v2, 0);
  }

  BVH bvh;
  bvh.build(tris.data(), N);

  REQUIRE(bvh.nodes.size() == 1);
  REQUIRE(bvh.nodes[0].primCount == N);
  REQUIRE(bvh.nodes[0].leftChild == -1);
}

// -----------------------------------------------------------------------
// empty BVH guards
// -----------------------------------------------------------------------

TEST_CASE("BVH traverse on empty BVH returns false") {
  BVH bvh;
  Ray r(Vec3(0,0,1), Vec3(0,0,-1));
  IntersectionData data;
  REQUIRE_FALSE(bvh.traverse(r, data));
}

TEST_CASE("BVH traverseVisible on empty BVH returns true") {
  BVH bvh;
  Ray r(Vec3(0,0,1), Vec3(0,0,-1));
  REQUIRE(bvh.traverseVisible(r, 10.0f));
}

// -----------------------------------------------------------------------
// traverse — hits and misses
// -----------------------------------------------------------------------

TEST_CASE("BVH traverse: ray miss returns false and leaves intersection unchanged") {
  auto tris = makeTriangleRow(1);
  BVH bvh;
  bvh.build(tris.data(), 1);

  // ray misses the triangle (x=2 is outside [0,1])
  Ray r(Vec3(2.0f, 0.5f, 1.0f), Vec3(0,0,-1));
  IntersectionData data;
  REQUIRE_FALSE(bvh.traverse(r, data));
  REQUIRE(data.t  == FLT_MAX);
  REQUIRE(data.ID == UINT_MAX);
}

TEST_CASE("BVH traverse: ray hit returns correct t and triangle ID") {
  auto tris = makeTriangleRow(1); // triangle 0: v0=(0,0,0), v1=(1,0,0), v2=(0,1,0)
  BVH bvh;
  bvh.build(tris.data(), 1);

  Ray r(Vec3(0.25f, 0.25f, 2.0f), Vec3(0,0,-1));
  IntersectionData data;
  REQUIRE(bvh.traverse(r, data));
  REQUIRE(data.t  == Catch::Approx(2.0f).margin(1e-4f));
  REQUIRE(data.ID == 0u);
}

TEST_CASE("BVH traverse: correct barycentric coordinates") {
  // Hit at (0.2, 0.5, 0): u=0.2 (beta), v=0.5 (gamma), alpha=0.3
  std::vector<Triangle> tris = { makeFlatTri(Vec3(0,0,0), Vec3(1,0,0), Vec3(0,1,0)) };
  BVH bvh;
  bvh.build(tris.data(), 1);

  Ray r(Vec3(0.2f, 0.5f, 1.0f), Vec3(0,0,-1));
  IntersectionData data;
  REQUIRE(bvh.traverse(r, data));
  REQUIRE(data.alpha == Catch::Approx(0.3f).margin(1e-4f));
  REQUIRE(data.beta  == Catch::Approx(0.2f).margin(1e-4f));
  REQUIRE(data.gamma == Catch::Approx(0.5f).margin(1e-4f));
}

TEST_CASE("BVH traverse: ray pointing away from scene returns false") {
  auto tris = makeTriangleRow(8);
  BVH bvh;
  bvh.build(tris.data(), 8);

  Ray r(Vec3(0.25f, 0.25f, 1.0f), Vec3(0,0,1)); // pointing away from z=0 plane
  IntersectionData data;
  REQUIRE_FALSE(bvh.traverse(r, data));
}

TEST_CASE("BVH traverse: returns closest hit among triangles in two BVH subtrees") {
  // z=0 layer (hit at t=1) and z=-5 layer (hit at t=6), ray hits both.
  // Must return the z=0 triangle (closer), not the z=-5 one.
  auto tris = makeTwoLayerScene();
  BVH bvh;
  bvh.build(tris.data(), (int)tris.size());

  Ray r(Vec3(0.25f, 0.25f, 1.0f), Vec3(0,0,-1));
  IntersectionData data;
  REQUIRE(bvh.traverse(r, data));
  REQUIRE(data.t == Catch::Approx(1.0f).margin(1e-4f));
  REQUIRE(data.ID == 0u); // triangle 0 is the z=0 layer hit
}

TEST_CASE("BVH traverse: correct triangle ID across full BVH tree") {
  // Build 32-triangle row, shoot rays at specific triangles and verify the
  // correct original ID is returned — covers traversal across many tree nodes.
  const int N = 32;
  auto tris = makeTriangleRow(N);
  BVH bvh;
  bvh.build(tris.data(), N);

  for (int target : {0, 7, 15, 20, 31}) {
    float x = target + 0.1f;
    Ray r(Vec3(x, 0.1f, 1.0f), Vec3(0,0,-1));
    IntersectionData data;
    REQUIRE(bvh.traverse(r, data));
    REQUIRE(data.ID == (unsigned int)target);
  }
}

// -----------------------------------------------------------------------
// traverseVisible
// -----------------------------------------------------------------------

TEST_CASE("BVH traverseVisible: unobstructed ray returns true") {
  auto tris = makeTwoLayerScene();
  BVH bvh;
  bvh.build(tris.data(), (int)tris.size());

  // ray offset to x=10, misses all triangles
  Ray r(Vec3(10.0f, 0.5f, 1.0f), Vec3(0,0,-1));
  REQUIRE(bvh.traverseVisible(r, 10.0f));
}

TEST_CASE("BVH traverseVisible: obstructed ray returns false") {
  auto tris = makeTwoLayerScene();
  BVH bvh;
  bvh.build(tris.data(), (int)tris.size());

  Ray r(Vec3(0.25f, 0.25f, 1.0f), Vec3(0,0,-1));
  REQUIRE_FALSE(bvh.traverseVisible(r, 10.0f));
}

TEST_CASE("BVH traverseVisible: maxT shorter than hit distance returns true") {
  auto tris = makeTwoLayerScene(); // z=0 hit at t=1
  BVH bvh;
  bvh.build(tris.data(), (int)tris.size());

  Ray r(Vec3(0.25f, 0.25f, 1.0f), Vec3(0,0,-1));
  REQUIRE(bvh.traverseVisible(r, 0.5f)); // shadow ray stops before triangle
}

TEST_CASE("BVH traverseVisible: maxT past hit distance returns false") {
  auto tris = makeTwoLayerScene(); // z=0 hit at t=1
  BVH bvh;
  bvh.build(tris.data(), (int)tris.size());

  Ray r(Vec3(0.25f, 0.25f, 1.0f), Vec3(0,0,-1));
  REQUIRE_FALSE(bvh.traverseVisible(r, 2.0f)); // shadow ray reaches triangle
}

TEST_CASE("BVH traverseVisible: ray starting on triangle surface is not self-occluded") {
  // In path tracing, shadow rays start at the shading point (on a surface).
  // A hit at t=0 must be ignored — otherwise the surface occludes itself.
  // The tHit > 0.0f guard in traverseVisibleNode handles this.
  std::vector<Triangle> tris = { makeFlatTri(Vec3(0,0,0), Vec3(1,0,0), Vec3(0,1,0)) };
  BVH bvh;
  bvh.build(tris.data(), 1);

  // Ray origin exactly on the triangle plane — MT returns t=0, should be ignored
  Ray r(Vec3(0.25f, 0.25f, 0.0f), Vec3(0,0,1));
  REQUIRE(bvh.traverseVisible(r, 10.0f));
}

TEST_CASE("BVH traverseVisible: correct across full BVH tree") {
  const int N = 32;
  auto tris = makeTriangleRow(N);
  BVH bvh;
  bvh.build(tris.data(), N);

  // ray aimed at triangle 15 — should be occluded
  Ray blocked(Vec3(15.1f, 0.1f, 1.0f), Vec3(0,0,-1));
  REQUIRE_FALSE(bvh.traverseVisible(blocked, 2.0f));

  // ray between triangles 15 and 16 — should be clear (x=15.999 is outside tri 15)
  // tri 15: v0=(15,0,0), v1=(16,0,0), v2=(15,1,0). x=15.999, y=0.001 → u=0.999, v=0.001 → u+v=1.0 → barely on edge
  // Use x well between triangles: triangle boundary is at exactly x=16
  Ray clear(Vec3(15.5f, 1.5f, 1.0f), Vec3(0,0,-1)); // y=1.5 is outside all triangles (y goes 0-1)
  REQUIRE(bvh.traverseVisible(clear, 2.0f));
}
