#ifndef GEOMETRY_H
#define GEOMETRY_H

#include "core.h"
#include "ray.h"
#include "sampling.h"

constexpr float RAY_EPSILON = 1e-6f;

class Triangle {
public:
  Vertex vertices[3];
	Vec3 e1; // Edge 1
	Vec3 e2; // Edge 2
	Vec3 n; // Geometric Normal

  float area;
	unsigned int materialIndex;
	Vec3 centroid;

  Triangle();
  void init(Vertex v0, Vertex v1, Vertex v2, unsigned int _materialIndex);
  Vec3 centre() const;
  bool rayIntersectMT(const Ray& r, float& t, float& u, float& v) const;
  
  void interpolateAttributes(
    const float alpha,
    const float beta,
    const float gamma,
    Vec3& interpolatedNormal,
    float& interpolatedU,
    float& interpolatedV
  ) const;

  Vec3 sample(Sampler* sampler, float& pdf);
  Vec3 gNormal() const;
};

class AABB {
public:
  Vec3 bmax;
  Vec3 bmin;

  AABB();
  void reset();
  void extend(const Vec3& p);
  bool rayAABB(const Ray& r, float& t) const;
  bool rayAABB(const Ray& r) const;
  float area() const;
};

#endif // GEOMETRY_H
