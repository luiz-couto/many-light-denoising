#include "shading.h"

bool BSDF::isLight() {
  return emission.lum() > 0;
}

void BSDF::addLight(Colour emission) {
  this->emission = emission;
}

Colour BSDF::emit(const ShadingData& shadingData, const Vec3& wi) {
  return emission;
}
