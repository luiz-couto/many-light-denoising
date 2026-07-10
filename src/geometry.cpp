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
