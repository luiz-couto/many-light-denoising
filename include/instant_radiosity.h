#ifndef INSTANT_RADIOSITY_H
#define INSTANT_RADIOSITY_H

#include <vector>
#include "integrator.h"
#include "vpl.h"

class InstantRadiosityIntegrator : public Integrator {
public:
  std::vector<VPL> vpls;

  using Integrator::Integrator;

  // photon pass: placing VPLs
  void prepare(Sampler* sampler) override;

  // gather pass: iteration over VPLs for each pixel
  Colour integrate(const Ray& ray, Sampler* sampler) override;

  // sum over vpls
  Colour gatherVPLs(const ShadingData& shadingData);
  Colour unshadowedVPLContribution(const ShadingData& shadingData, const VPL& vpl);

  // sample light + position + direction; returns initial photon flux, fills the emitted ray
  Colour emitPhoton(Sampler* sampler, Ray& emittedRay);

  // deposit VPL and gets its radiance
  void depositVPL(const ShadingData& shadingData, const Colour& flux, const Colour& albedoAtHit);
};

#endif // INSTANT_RADIOSITY_H
