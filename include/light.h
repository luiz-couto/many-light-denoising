#ifndef LIGHT_H
#define LIGHT_H

#include "core.h"
#include "sampling.h"
#include "shading.h"
#include "geometry.h"

class Light {
public:
	virtual ~Light() = default;
	virtual Vec3 sample(const ShadingData& shadingData, Sampler* sampler, Colour& emittedColour, float& pdf) = 0;
	virtual Colour evaluate(const Vec3& wi) = 0;
	virtual float PDF(const ShadingData& shadingData, const Vec3& wi) = 0;
	virtual bool isArea() = 0;
	virtual Vec3 normal(const ShadingData& shadingData, const Vec3& wi) = 0;
	virtual float totalIntegratedPower() = 0;
	virtual Vec3 samplePositionFromLight(Sampler* sampler, float& pdf);
	virtual Vec3 sampleDirectionFromLight(Sampler* sampler, float& pdf) = 0;
};

class AreaLight : public Light {
public:
  Triangle* triangle = NULL;
	Colour emission;

  Vec3 sample(const ShadingData& shadingData, Sampler* sampler, Colour& emittedColour, float& pdf);
  Colour evaluate(const Vec3& wi);
  float PDF(const ShadingData& shadingData, const Vec3& wi);
  bool isArea();
  Vec3 normal(const ShadingData& shadingData, const Vec3& wi);
  float totalIntegratedPower();
  Vec3 samplePositionFromLight(Sampler* sampler, float& pdf);
  Vec3 sampleDirectionFromLight(Sampler* sampler, float& pdf);
};

class BackgroundColour : public Light {
public:
  Colour emission;

  BackgroundColour(Colour _emission);

  Vec3 sample(const ShadingData& shadingData, Sampler* sampler, Colour& reflectedColour, float& pdf);
  Colour evaluate(const Vec3& wi);
  float PDF(const ShadingData& shadingData, const Vec3& wi);
  bool isArea();
  Vec3 normal(const ShadingData& shadingData, const Vec3& wi);
  float totalIntegratedPower();
  Vec3 sampleDirectionFromLight(Sampler* sampler, float& pdf);
};

#endif // LIGHT_H
