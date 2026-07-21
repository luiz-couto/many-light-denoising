#ifndef BVH_H
#define BVH_H

#include <vector>
#include "geometry.h"

constexpr int BUILD_BINS = 8;
constexpr float BOUNDS_COST  = 1.0f;
constexpr float C_ISECT_COST = 1.0f;
constexpr int MAXLEAF_TRIANGLES = 4;

struct Bin {
  AABB bounds;
  int numTriangles;
};

struct SplitResult {
  float cost;
  int   axis;
  float splitPos;
};

struct BVHNode {
  AABB bounds;
  int leftChild; // right child is leftChild + 1
  int firstPrim; // the idx in the primitives arr
  int primCount; // if > 0, is leaf
};

class BVH {
public:
  std::vector<BVHNode> nodes;
  std::vector<int> primIndices;
  Triangle* triangles;

  // populate bins for one axis
  std::vector<Bin> binTriangles(int start, int count, int axis, float minInAxis, float maxInAxis) const;

  // left/right sweep over bins, return best split cost and position for this axis
  SplitResult sweepBins(const std::vector<Bin>& bins, int axis, float parentArea, float minInAxis, float stepSize) const;

  // try all 3 axes, return best split
  SplitResult findBestSplit(int start, int count, float parentArea) const;

  // orchestrates: compute bounds → find split → leaf-or-recurse
  void buildNode(int nodeIdx, int start, int count);

  BVH();
  void build(Triangle* triangles, int count);
  bool traverse(const Ray& ray, IntersectionData& intersection) const;
  bool traverseVisible(const Ray& ray, float maxT) const;
};

#endif // BVH_H
