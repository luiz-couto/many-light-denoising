#ifndef INTEGRATOR_H
#define INTEGRATOR_H

#include "scene.h"
#include "film.h"

struct LightSamplingMISResult {
  Colour finalColor;
  Vec3 wi;
  float pdf;
  float cosThetaLine = 0;
  float distSqr = 0;
  float gTerm = 0;
};

class Integrator {
public:
  Scene* scene;
  Film*  film;
  
  Integrator(Scene* scene, Film* film);
  virtual ~Integrator() = default;

  // Default: runs tile loop, calling integrate() per pixel.
  // Self-rendering algorithms (MLT, Light Tracing, BDPT) override this
  // entirely and ignore integrate().
  virtual void render();
  virtual void prepare(Sampler* sampler) {} // no-op by default
  void renderTile(int threadId, std::atomic<unsigned int>& tileId, MTRandom& sampler);

  // Only needs implementing for tile-based algorithms.
  // Not pure-virtual: self-rendering algorithms leave this as a no-op.
  virtual Colour integrate(const Ray& ray, Sampler* sampler) { return {}; }

  Colour computeDirectMIS(const ShadingData& sd, Sampler* sampler);
  Colour computeDirectBSDFMIS(const ShadingData& shadingData, Sampler* sampler);
  void lightSamplingMIS(ShadingData shadingData, Sampler* sampler, Colour &result);
  LightSamplingMISResult lightSamplingMISAreaLight(ShadingData shadingData, Sampler* sampler, Light* sampledLight, float pmf);
  LightSamplingMISResult lightSamplingMISEnvMap(ShadingData shadingData, Sampler* sampler, Light* sampledLight, float pmf);
};

#endif // INTEGRATOR_H
