#ifndef RESTIR_INSTANT_RADIOSITY_H
#define RESTIR_INSTANT_RADIOSITY_H

#include "instant_radiosity.h"
#include "reservoir.h"
#include <functional>
#include <vector>

struct PrimaryHit {
  ShadingData shadingData;   // the gather point (after any specular chain)
  Colour throughput;         // tint accumulated through mirror/glass
  Colour resolved;           // miss/emissive/chain-exceeded result
  bool needsGather = false;  // if false, just splat 'resolved'
};

class ReSTIRInstantRadiosityIntegrator : public InstantRadiosityIntegrator {
public:
  std::vector<PrimaryHit> gBuffer;
  std::vector<Reservoir> reservoirs;
  std::vector<Reservoir> reservoirsPrev;

  using InstantRadiosityIntegrator::InstantRadiosityIntegrator;

  // One full pass: prepare (photon pass) + the three tiled phases + incrementSPP.
  void render() override;
  void runTiled(const std::function<void(int x, int y)>& pixelFunc);
  void runPixelFunc(std::atomic<unsigned int>& tileId, const std::function<void(int x, int y)>& pixelFunc);

  // Phase 1a: camera ray (walking through pure speculars), fills the G-buffer.
  void tracePrimary(int x, int y, Sampler* sampler);

  float pHat(const ShadingData& shadingData, const VPL& vpl);

  // Phase 1b: M uniform candidates scored by pHat, streamed into
  // the pixel's reservoir, then finalized. Arithmetic only, no rays.
  void generateCandidates(int pixelIndex, Sampler* sampler);

  // Phase 2 (per round): merge K random neighbours within the reuse radius,
  // re-scoring their winners with THIS pixel's pHat; re-finalize afterwards.
  void spatialReuse(int x, int y, int round, Sampler* sampler);
  int selectRandomNeighbour(int x, int y, Sampler* sampler);

  // Phase 3: direct MIS (NEE + BSDF-sampling branch) + winner contribution
  // * W * visibility, one shadow ray each. Returns the colour to splat.
  Colour shadePixel(int pixelIndex, Sampler* sampler);
};

#endif // RESTIR_INSTANT_RADIOSITY_H
