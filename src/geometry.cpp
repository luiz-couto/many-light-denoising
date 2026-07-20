#include "geometry.h"
#include <cmath>

Triangle::Triangle() {}

void Triangle::init(Vertex v0, Vertex v1, Vertex v2, unsigned int _materialIndex) {
  materialIndex = _materialIndex;
  vertices[0] = v0;
  vertices[1] = v1;
  vertices[2] = v2;

  e1 = vertices[1].p - vertices[0].p;
  e2 = vertices[2].p - vertices[0].p;

  Vec3 c = e1.cross(e2);
  n    = c.normalize();
  area = c.length() * 0.5f;
  centroid = centre();
}

Vec3 Triangle::centre() const {
  return (vertices[0].p + vertices[1].p + vertices[2].p) / 3.0f;
}

bool Triangle::rayIntersectMT(const Ray& r, float& t, float& u, float& v) const {
  Vec3 p = cross(r.dir, e2);
  float det = dot(e1, p);

  if (std::abs(det) < RAY_EPSILON) return false;

  float invDet = 1.0f / det;
  Vec3 T = r.o - vertices[0].p;

  u = dot(T, p) * invDet;
  if (u < 0.0f || u > 1.0f) return false;

  Vec3 q = cross(T, e1);
  v = dot(r.dir, q) * invDet;
  if (v < 0.0f || u + v > 1.0f) return false;

  t = dot(e2, q) * invDet;
  return t > 0.0f;
}

void Triangle::interpolateAttributes(const float alpha, const float beta, const float gamma, Vec3& interpolatedNormal, float& interpolatedU, float& interpolatedV) const {
  interpolatedNormal = vertices[0].normal * alpha + vertices[1].normal * beta + vertices[2].normal * gamma;
  interpolatedNormal = interpolatedNormal.normalize();
  interpolatedU = vertices[0].u * alpha + vertices[1].u * beta + vertices[2].u * gamma;
  interpolatedV = vertices[0].v * alpha + vertices[1].v * beta + vertices[2].v * gamma;
}

Vec3 Triangle::gNormal() const {
  return (n * (dot(vertices[0].normal, n) > 0 ? 1.0f : -1.0f));
}

Vec3 Triangle::sample(Sampler* sampler, float& pdf) {
  pdf = 1.0f / area;
  float r1 = sampler->next();
  float r2 = sampler->next();

  float sqrtR1 = sqrtf(r1);
  float alpha = 1.0f - sqrtR1;
  float beta  = r2 * sqrtR1;
  float gamma = 1.0f - (alpha + beta);

  return vertices[0].p * alpha + vertices[1].p * beta + vertices[2].p * gamma;
}

AABB::AABB() {
  reset();
}

void AABB::reset() {
  bmax = Vec3(-FLT_MAX, -FLT_MAX, -FLT_MAX);
	bmin = Vec3(FLT_MAX, FLT_MAX, FLT_MAX);
}

void AABB::extend(const Vec3& p) {
  bmax = maxVec(bmax, p);
	bmin = minVec(bmin, p);
}

bool AABB::rayAABB(const Ray& r, float& t) const {
  Vec3 tMin = (bmin - r.o) * (r.invDir);
  Vec3 tMax = (bmax - r.o) * (r.invDir);

  Vec3 tEntry = minVec(tMin, tMax);
  Vec3 tExit = maxVec(tMin, tMax);

  float entryVal = std::max({ tEntry.x, tEntry.y, tEntry.z });
  float exitVal = std::min({ tExit.x, tExit.y, tExit.z });

  t = entryVal;
  return (entryVal <= exitVal && exitVal > 0);
}

bool AABB::rayAABB(const Ray& r) const {
  float t;
  return rayAABB(r, t);
}

float AABB::area() const {
  Vec3 size = bmax - bmin;
	return ((size.x * size.y) + (size.y * size.z) + (size.x * size.z)) * 2.0f;
}
