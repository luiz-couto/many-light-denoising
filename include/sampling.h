#ifndef SAMPLING_H
#define SAMPLING_H

#include "core.h"
#include <random>

class Sampler {
public:
  virtual float next() = 0;
  virtual ~Sampler() = default;
};

class MTRandom : public Sampler {
public:
  std::mt19937 generator;
	std::uniform_real_distribution<float> dist;

  MTRandom(unsigned int seed = 1);
  float next();
};

// Note that all of these distributions assume z-up coordinate system
class SamplingDistributions {
public:
  static Vec3 uniformSampleHemisphere(float r1, float r2);
  static Vec3 cosineSampleHemisphereByDisk(float r1, float r2);
  static float uniformHemispherePDF(const Vec3& wi);

  static Vec3 cosineSampleHemisphere(float r1, float r2);
  static float cosineHemispherePDF(const Vec3& wi);

  static Vec3 uniformSampleSphere(float r1, float r2);
  static float uniformSpherePDF(const Vec3& wi);
};

#endif // SAMPLING_H
