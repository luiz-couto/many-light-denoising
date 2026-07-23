#ifndef SHADING_H
#define SHADING_H

#include "core.h"
#include "sampling.h"
#include <cfloat>

class BSDF;

class ShadingData {
public:
	Vec3 x;
	Vec3 wo;
	Vec3 sNormal;
	Vec3 gNormal;
	float tu = 0.0f;
	float tv = 0.0f;
	Frame frame;
	float t = FLT_MAX;
	BSDF* bsdf = nullptr;

	ShadingData() {}
	ShadingData(Vec3 _x, Vec3 n): x(_x), gNormal(n), sNormal(n) {
		frame.fromVector(sNormal);
		bsdf = nullptr;
	}
};

class BSDF {
public:
	Colour emission;

	virtual ~BSDF() = default;
	virtual Vec3 sample(const ShadingData& shadingData, Sampler* sampler, Colour& reflectedColour, float& pdf) = 0;
	virtual Colour evaluate(const ShadingData& shadingData, const Vec3& wi) = 0;
	virtual float PDF(const ShadingData& shadingData, const Vec3& wi) = 0;
	virtual bool isPureSpecular() = 0;
	virtual bool isTwoSided() = 0;
	virtual float mask(const ShadingData& shadingData) = 0;

	bool isLight();
	void addLight(Colour _emission);
	Colour emit(const ShadingData& shadingData, const Vec3& wi);
};

#endif // SHADING_H
