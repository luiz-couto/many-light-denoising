#ifndef PATH_TRACER_H
#define PATH_TRACER_H

#include "integrator.h"

class PathTracerIntegrator : public Integrator {
public:
  static constexpr int MAX_DEPTH = 5;
  static constexpr int RR_DEPTH  = 3;

  using Integrator::Integrator;
  Colour integrate(const Ray& ray, Sampler* sampler) override;
  Colour pathTrace(const Ray& ray, Colour throughput, int depth, float bsdfPDF, Sampler* sampler, bool isSpecularBounce);
};

#endif // PATH_TRACER_H
