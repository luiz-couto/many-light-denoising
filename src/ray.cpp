#include "ray.h"

Ray::Ray() {}

Ray::Ray(Vec3 _o, Vec3 _d) {
	init(_o, _d);
}

void Ray::init(Vec3 _o, Vec3 _d) {
  o = _o;
  dir = _d;
  invDir = Vec3(1.0f / dir.x, 1.0f / dir.y, 1.0f / dir.z);
}

Vec3 Ray::at(const float t) const {
	return (o + (dir * t));
}
