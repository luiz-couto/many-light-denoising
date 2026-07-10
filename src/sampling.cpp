#include "sampling.h"
#include <algorithm>

MTRandom::MTRandom(unsigned int seed) : dist(0.0f, 1.0f) {
  generator.seed(seed);
}

float MTRandom::next() {
  return dist(generator);
}

Vec3 SamplingDistributions::uniformSampleHemisphere(float r1, float r2) {
  float theta = acosf(r1);
	float phi = 2 * PI * r2;
	return SphericalCoordinates::sphericalToWorld(theta, phi);
}

float SamplingDistributions::uniformHemispherePDF(const Vec3& wi) {
  return 1.0f / (2 * PI);
}

Vec3 SamplingDistributions::cosineSampleHemisphereByDisk(float r1, float r2) {
  float r = sqrtf(r1);
  float theta = 2 * PI * r2;
  float x = r * cosf(theta);
  float y = r * sinf(theta);
  float z = sqrtf(std::max(0.0f, 1.0f - x * x - y * y));
  return Vec3(x, y, z);
}

Vec3 SamplingDistributions::cosineSampleHemisphere(float r1, float r2) {
  float theta = acosf(sqrtf(r1));
  float phi = 2 * PI * r2;
  return SphericalCoordinates::sphericalToWorld(theta, phi);
}

float SamplingDistributions::cosineHemispherePDF(const Vec3& wi) {
  return std::max(0.0f, wi.z) / PI;
}

Vec3 SamplingDistributions::uniformSampleSphere(float r1, float r2) {
  float theta = acosf(1 - (2 * r1));
  float phi = 2 * PI * r2;
  return SphericalCoordinates::sphericalToWorld(theta, phi);
}

float SamplingDistributions::uniformSpherePDF(const Vec3& wi) {
  return 1.0f / (4 * PI);
}
