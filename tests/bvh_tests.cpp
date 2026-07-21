#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <algorithm>
#include "bvh.h"

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
