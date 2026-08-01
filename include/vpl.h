#ifndef VPL_H
#define VPL_H

#include "core.h"

struct VPL {
  Vec3 position;
  Vec3 normal;
  Colour radiance;
  float footprintRadius = 0.0f;
};

#endif // VPL_H
