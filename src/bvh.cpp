#include "bvh.h"
#include <algorithm>
#include <numeric>

BVH::BVH() {}

void BVH::build(Triangle* triangles, int count) {
  this->triangles = triangles;
  primIndices.resize(count);
  std::iota(primIndices.begin(), primIndices.end(), 0);

  nodes.emplace_back(); // allocate root node
  buildNode(0, 0, count);
}

// Recursively builds a BVH node for the triangle range [start, start+count).
// First computes the node's AABB from all triangle vertices in the range.
// Then evaluates the SAH to find the best split. If the split cost is worse
// than a leaf (count * C_ISECT_COST), or the count is below MAXLEAF_TRIANGLES,
// or no valid split exists, the node becomes a leaf storing [start, start+count).
// Otherwise, partitions primIndices around the split position, allocates both
// child nodes before recursing (to avoid vector reallocation invalidating indices),
// and recurses into left and right children.
void BVH::buildNode(int nodeIdx, int start, int count) {
  for (int i = start; i < (start + count); i++) {
    nodes[nodeIdx].bounds.extend(triangles[primIndices[i]].vertices[0].p);
    nodes[nodeIdx].bounds.extend(triangles[primIndices[i]].vertices[1].p);
    nodes[nodeIdx].bounds.extend(triangles[primIndices[i]].vertices[2].p);
	}

  SplitResult split = findBestSplit(start, count, nodes[nodeIdx].bounds.area());
  float leafCost = count * C_ISECT_COST;

  if (count <= MAXLEAF_TRIANGLES || split.cost >= leafCost) {
    nodes[nodeIdx].firstPrim = start;
    nodes[nodeIdx].primCount = count;
    nodes[nodeIdx].leftChild = -1;
    return;
  }

  // partition primIndices around the split position
  auto mid = std::partition(
    primIndices.begin() + start,
    primIndices.begin() + start + count,
    [&](int idx) {
      return triangles[idx].centroid.coords[split.axis] < split.splitPos;
    }
  );

  int leftCount  = mid - (primIndices.begin() + start);
  int rightCount = count - leftCount;

  // all triangles fell on one side, force leaf
  if (leftCount == 0 || rightCount == 0) {
    nodes[nodeIdx].firstPrim = start;
    nodes[nodeIdx].primCount = count;
    nodes[nodeIdx].leftChild = -1;
    return;
  }

  // allocate both children before recursing
  int leftIdx = nodes.size();
  nodes.emplace_back();
  nodes.emplace_back();

  nodes[nodeIdx].leftChild = leftIdx;
  nodes[nodeIdx].primCount = 0; // interior node

  buildNode(leftIdx, start, leftCount);
  buildNode(leftIdx + 1, start + leftCount, rightCount);
}

// Divides the triangle range into BUILD_BINS equal-sized slots along one axis, then figures out 
// which slot each triangle falls into. First, Allocates BUILD_BINS bins, each with an empty AABB
// and zero triangle count. The bins divide the axis range [minInAxis, maxInAxis] into equal
// intervals of size stepSize. For each triangle in the range, it looks at its centroid
// coordinate along the given axis and computes which bin it falls into with the direct formula:
// binIdx = (centroid - minInAxis) / stepSize; 
// Then it increments that bin's triangle count and expands that bin's AABB to include the
// triangle's three vertices.
std::vector<Bin> BVH::binTriangles(int start, int count, int axis, float minInAxis, float maxInAxis) const {
  float stepSize = (maxInAxis - minInAxis) / float(BUILD_BINS);
  std::vector<Bin> bins(BUILD_BINS, {AABB(), 0});

  for (int i = start; i < start + count; i++) {
    int idx = primIndices[i];
    float centroid = triangles[idx].centroid.coords[axis];
    int binIdx = std::min((int)((centroid - minInAxis) / stepSize), BUILD_BINS - 1);

    bins[binIdx].numTriangles++;
    bins[binIdx].bounds.extend(triangles[idx].vertices[0].p);
    bins[binIdx].bounds.extend(triangles[idx].vertices[1].p);
    bins[binIdx].bounds.extend(triangles[idx].vertices[2].p);
  }

  return bins;
}

// Finds the best SAH split for the triangles in [start, start+count).
// Builds centroid bounds to define bin ranges, then for each axis bins the
// triangles, sweeps left/right to evaluate SAH costs, and tracks the cheapest
// split across all three axes. Returns the best axis, split position, and cost.
// parentArea is the surface area of the current node's AABB, used to normalise
// the SAH cost. Returns cost=FLT_MAX if no valid split exists (all centroids coplanar).
SplitResult BVH::findBestSplit(int start, int count, float parentArea) const {
  AABB centroidBounds;
  for (int i = start; i < start + count; i++) {
    centroidBounds.extend(triangles[primIndices[i]].centroid);
  }

  SplitResult best = { FLT_MAX, 0, 0.0f };

  // For each axis (0 = x, 1 = y, 2 = z)
  for (int axis = 0; axis < 3; axis++) {
    float minInAxis = centroidBounds.bmin.coords[axis];
    float maxInAxis = centroidBounds.bmax.coords[axis];

    if (minInAxis == maxInAxis) continue; // all centroids coplanar on this axis
    float stepSize = (maxInAxis - minInAxis) / float(BUILD_BINS);

    std::vector<Bin> bins = binTriangles(start, count, axis, minInAxis, maxInAxis);
    SplitResult candidate = sweepBins(bins, axis, parentArea, minInAxis, stepSize);

    if (candidate.cost < best.cost) {
      best = candidate;
    }
  }

  return best;
}

// Evaluates SAH split costs across all BUILD_BINS-1 candidate split positions for a given axis.
// Builds a left-to-right prefix sweep (cumulative AABB and count from bin 0 upward) and a
// right-to-left prefix sweep (from bin BUILD_BINS-1 downward). For each candidate split i,
// the left side covers bins [0..i] and the right side covers bins [i+1..BUILD_BINS-1].
// SAH cost = BOUNDS_COST + (leftArea/parentArea * leftCount + rightArea/parentArea * rightCount) * C_ISECT_COST; 
// Returns the SplitResult with the lowest cost, including its world-space split position
// computed as minInAxis + (i+1) * stepSize. Returns cost=FLT_MAX if no valid split exists.
SplitResult BVH::sweepBins(const std::vector<Bin>& bins, int axis, float parentArea, float minInAxis, float stepSize) const {
  auto buildSweep = [&](int from, int to, int step) {
    std::vector<Bin> sweep(BUILD_BINS);
    int total = 0;
    AABB acc;
    for (int i = from; i != to; i += step) {
      total += bins[i].numTriangles;
      if (bins[i].numTriangles > 0) {
        acc.extend(bins[i].bounds.bmin);
        acc.extend(bins[i].bounds.bmax);
      }
      sweep[i] = { acc, total };
    }
    return sweep;
  };

  auto leftSweep = buildSweep(0, BUILD_BINS, 1);
  auto rightSweep = buildSweep(BUILD_BINS - 1, -1, -1);

  SplitResult best = { FLT_MAX, axis, 0.0f };

  for (int i = 0; i < BUILD_BINS - 1; i++) {
    if (leftSweep[i].numTriangles == 0 || rightSweep[i + 1].numTriangles == 0) continue;

    float leftCost  = (leftSweep[i].bounds.area() / parentArea) * leftSweep[i].numTriangles;
    float rightCost = (rightSweep[i + 1].bounds.area() / parentArea) * rightSweep[i + 1].numTriangles;
    float cost = BOUNDS_COST + (leftCost + rightCost) * C_ISECT_COST;

    if (cost < best.cost) {
      best.cost = cost;
      best.splitPos = minInAxis + (i + 1) * stepSize;
    }
  }

  return best;
}

bool BVH::traverse(const Ray& ray, IntersectionData& intersection) const {
  if (nodes.empty()) return false;
  return traverseNode(0, ray, intersection);
}

bool BVH::traverseNode(int nodeIdx, const Ray& ray, IntersectionData& intersection) const {
  const BVHNode& node = nodes[nodeIdx];
  float t;

  // the ray completely misses this node's bounding box. No point descending into it
  if (!node.bounds.rayAABB(ray, t) || t > intersection.t) return false;

  // leaf
  if (node.primCount > 0) {
    bool hit = false;
    for (int i = node.firstPrim; i < node.firstPrim + node.primCount; i++) {
      float tHit, u, v;
      if (triangles[primIndices[i]].rayIntersectMT(ray, tHit, u, v) && tHit < intersection.t) {
        intersection.t = tHit;
        intersection.ID = primIndices[i];
        intersection.alpha = 1.0f - u - v;
        intersection.beta  = u;
        intersection.gamma = v;
        hit = true;
      }
    }
    return hit;
  }

  // interior
  // tests both children's bounding boxes to figure out which is nearer, then recurses nearer-first
  int leftIdx  = node.leftChild;
  int rightIdx = node.leftChild + 1;
  float tLeft = FLT_MAX, tRight = FLT_MAX;

  nodes[leftIdx].bounds.rayAABB(ray, tLeft);
  nodes[rightIdx].bounds.rayAABB(ray, tRight);

  bool hit = false;
  int nearestNode = tLeft < tRight ? leftIdx : rightIdx;
  int farNode = tLeft < tRight ? rightIdx : leftIdx;
  
  hit = traverseNode(nearestNode,  ray, intersection);
  hit |= traverseNode(farNode, ray, intersection);

  return hit;
}

bool BVH::traverseVisible(const Ray& ray, float maxT) const {
  if (nodes.empty()) return true;
  return traverseVisibleNode(0, ray, maxT);
}

bool BVH::traverseVisibleNode(int nodeIdx, const Ray& ray, float maxT) const {
  const BVHNode& node = nodes[nodeIdx];
  float t;

  // the ray completely misses this node's bounding box. No point descending into it
  if (!node.bounds.rayAABB(ray, t) || t > maxT) return true;

  // leaf
  if (node.primCount > 0) { 
    for (int i = node.firstPrim; i < node.firstPrim + node.primCount; i++) {
      float tHit, u, v;
      if (triangles[primIndices[i]].rayIntersectMT(ray, tHit, u, v) && tHit > 0.0f && tHit < maxT) {
        return false; // occluded
      }
    }
    return true;
  }

  // interior
  // tests both children's bounding boxes to figure out which is nearer, then recurses nearer-first
  int leftIdx  = node.leftChild;
  int rightIdx = node.leftChild + 1;
  float tLeft = FLT_MAX, tRight = FLT_MAX;

  nodes[leftIdx].bounds.rayAABB(ray, tLeft);
  nodes[rightIdx].bounds.rayAABB(ray, tRight);

  int nearestNode = tLeft < tRight ? leftIdx : rightIdx;
  int farNode = tLeft < tRight ? rightIdx : leftIdx;
  
  if (!traverseVisibleNode(nearestNode, ray, maxT)) return false;
  if (!traverseVisibleNode(farNode, ray, maxT)) return false;
  
  return true;
}
