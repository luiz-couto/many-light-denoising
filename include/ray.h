#ifndef RAY_H
#define RAY_H

#include "core.h"
#include <climits>
#include <cfloat>

class Ray {
public:
  Vec3 o;
	Vec3 dir;
	Vec3 invDir;

  Ray();
  Ray(Vec3 _o, Vec3 _d);
  void init(Vec3 _o, Vec3 _d);

  // Computes the 3D point along the ray at distance t
  Vec3 at(const float t) const;
};

struct IntersectionData {
  unsigned int ID = UINT_MAX;
  float t         = FLT_MAX;
  float alpha     = 0.0f;
  float beta      = 0.0f;
  float gamma     = 0.0f;
};

#endif // RAY_H
