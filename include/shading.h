#ifndef SHADING_H
#define SHADING_H

#include <cfloat>
#include "core.h"
#include "sampling.h"
#include "texture.h"

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

namespace ShadingHelper {
  // Computes the cosine of the transmitted angle via Snell's law.
  // n = extIOR / intIOR. Sets totalInternalReflection=true if TIR occurs.
  float getCosThetaT(float cosTheta, float n, bool& totalInternalReflection);

  // Fresnel reflectance for a dielectric (real IOR, transparent medium).
  // Returns intensity (amplitude²), averaged over s and p polarisations.
  // Uses both the incident and transmitted angles (transmitted via Snell's law internally).
  float fresnelDielectric(float cosTheta, float intIOR, float extIOR);

  // Overload taking the IOR ratio n = n_t/n_i directly (same convention as the two-IOR overload).
  float fresnelDielectric(float cosTheta, float n);

  // Fresnel intensity reflectance (already squared) for a conductor (complex IOR n+ik).
  // Unlike the dielectric overloads, there is no transmitted angle, conductors are opaque.
  // k is the extinction coefficient: k=0 matches the dielectric formula at normal incidence only.
  float fresnelConductorPerpendicularSqr(float cosTheta, float n, float k); // s-polarisation
  float fresnelConductorParallelSqr(float cosTheta, float n, float k);      // p-polarisation

  // Per-channel conductor Fresnel reflectance averaged over both polarisations.
  // ior and k are the real and imaginary parts of the complex refractive index, per RGB channel.
  Colour fresnelConductor(float cosTheta, Colour ior, Colour k);

  // GGX lambda function, helper for the Smith geometry term.
  // wi must be in the shading frame (normal = (0,0,1), wi.z = cosTheta).
  float lambdaGGX(Vec3 wi, float alpha);

  // Smith height-correlated masking-shadowing term for GGX.
  // wi and wo must be in the shading frame (normal = (0,0,1)).
  float Gggx(Vec3 wi, Vec3 wo, float alpha);

  // GGX normal distribution function evaluated at half-vector h.
  // h must be in the shading frame (normal = (0,0,1)).
  float Dggx(Vec3 h, float alpha);

  // Specular reflection of wo about the surface normal in the shading frame.
  Vec3 reflect(Vec3 wo_local);

  // Snell's law refraction in the shading frame.
  // n = n_t/n_i (same convention as fresnelDielectric). Returns the refracted direction,
  // or the reflected direction when total internal reflection occurs (tir set to true).
  Vec3 refract(Vec3 wo_local, float n, bool& tir);
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

class DiffuseBSDF : public BSDF {
public:
  Texture* albedo;

  DiffuseBSDF() = default;
  DiffuseBSDF(Texture* _albedo);

  Vec3 sample(const ShadingData& shadingData, Sampler* sampler, Colour& reflectedColour, float& pdf) override;
  Colour evaluate(const ShadingData& shadingData, const Vec3& wi) override;
  float PDF(const ShadingData& shadingData, const Vec3& wi) override;
  bool isPureSpecular() override;
  bool isTwoSided() override;
  float mask(const ShadingData& shadingData) override;
};

class MirrorBSDF : public BSDF {
public:
  Texture* albedo;
  Colour eta;
  Colour k;

  MirrorBSDF() = default;
  MirrorBSDF(Texture* _albedo, Colour _eta, Colour _k);

  Vec3 sample(const ShadingData& shadingData, Sampler* sampler, Colour& reflectedColour, float& pdf) override;
  Colour evaluate(const ShadingData& shadingData, const Vec3& wi) override;
  float PDF(const ShadingData& shadingData, const Vec3& wi) override;
  bool isPureSpecular() override;
  bool isTwoSided() override;
  float mask(const ShadingData& shadingData) override;
};

class GlassBSDF : public BSDF {
public:
  Texture* albedo;
	float intIOR;
	float extIOR;

  GlassBSDF() = default;
	GlassBSDF(Texture* _albedo, float _intIOR, float _extIOR);

  Vec3 sample(const ShadingData& shadingData, Sampler* sampler, Colour& reflectedColour, float& pdf) override;
  Colour evaluate(const ShadingData& shadingData, const Vec3& wi) override;
  float PDF(const ShadingData& shadingData, const Vec3& wi) override;
  bool isPureSpecular() override;
  bool isTwoSided() override;
  float mask(const ShadingData& shadingData) override;
};

class ConductorBSDF : public BSDF {
public:
  Texture* albedo;
	Colour eta;
	Colour k;
	float alpha;

  ConductorBSDF() = default;
	ConductorBSDF(Texture* _albedo, Colour _eta, Colour _k, float roughness);

  Vec3 sample(const ShadingData& shadingData, Sampler* sampler, Colour& reflectedColour, float& pdf) override;
  Colour evaluate(const ShadingData& shadingData, const Vec3& wi) override;
  float PDF(const ShadingData& shadingData, const Vec3& wi) override;
  bool isPureSpecular() override;
  bool isTwoSided() override;
  float mask(const ShadingData& shadingData) override;
};

class PlasticBSDF : public BSDF {
public:
  Texture* albedo;
	float intIOR;
	float extIOR;
	float alpha;

  PlasticBSDF() = default;
	PlasticBSDF(Texture* _albedo, float _intIOR, float _extIOR, float roughness);

  Vec3 sample(const ShadingData& shadingData, Sampler* sampler, Colour& reflectedColour, float& pdf) override;
  Colour evaluate(const ShadingData& shadingData, const Vec3& wi) override;
  float PDF(const ShadingData& shadingData, const Vec3& wi) override;
  bool isPureSpecular() override;
  bool isTwoSided() override;
  float mask(const ShadingData& shadingData) override;
};

#endif // SHADING_H
