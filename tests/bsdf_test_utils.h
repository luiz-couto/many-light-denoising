#pragma once

// Shared statistical validation helpers for BSDF unit tests.
// Used by: diffuse, mirror, conductor, glass, plastic test files.

#include "shading.h"
#include "sampling.h"
#include <cmath>
#include <vector>
#include <algorithm>

// ShadingData with surface normal (0,0,1) and the given outgoing direction.
static ShadingData makeTestSD(Vec3 wo) {
  ShadingData sd(Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f));
  sd.wo = wo;
  sd.tu = sd.tv = 0.0f;
  return sd;
}

// Estimates ∫ f(wo,wi)·cosθi dωi using N samples from bsdf.sample().
// sample() returns f·cosθ/pdf as reflectedColour, so its expectation equals
// the hemispherical reflectance. Energy-conserving BSDFs must have this ≤ 1.
//
// Divides by N (all draws), not by valid count. Rejected samples (pdf=0)
// contribute 0 and must be counted — otherwise the estimate is inflated by
// 1/P(accept), which for GGX at grazing angles can be >> 1.
static float estimateReflectance(BSDF* bsdf, const ShadingData& sd, int N = 2000) {
  MTRandom sampler;
  float sum = 0.0f;
  for (int i = 0; i < N; i++) {
    Colour col; float pdf;
    bsdf->sample(sd, &sampler, col, pdf);
    if (pdf > 0.0f && std::isfinite(col.lum())) sum += col.lum();
  }
  return sum / N;
}

// Chi-square test: verifies that bsdf.sample() produces a direction
// distribution consistent with bsdf.PDF(). Uses 16×16 equal-dω bins
// in (cosθ, φ) on the upper hemisphere. Only valid for non-delta BSDFs.
//
// Threshold uses 5σ above the chi-square mean (= dof), which corresponds to
// p ≈ 1e-6 under the null — essentially zero false positives for a correct BSDF.
static bool chiSquareTest(BSDF* bsdf, const ShadingData& sd, int N = 50000) {
  const int BT = 16, BP = 16, BINS = BT * BP;
  std::vector<int>   observed(BINS, 0);
  std::vector<float> expected(BINS, 0.0f);

  MTRandom sampler;
  int validSamples = 0;
  for (int i = 0; i < N; i++) {
    Colour col; float pdf;
    Vec3 wi = bsdf->sample(sd, &sampler, col, pdf);
    Vec3 local = sd.frame.toLocal(wi);
    if (local.z <= 0.0f || pdf <= 0.0f) continue;
    float cosT = std::clamp(local.z, 0.0f, 1.0f);
    float phi  = atan2f(local.y, local.x);
    if (phi < 0.0f) phi += 2.0f * M_PI;
    int ti = std::min((int)(cosT * BT), BT - 1);
    int pi = std::min((int)(phi / (2.0f * M_PI) * BP), BP - 1);
    observed[ti * BP + pi]++;
    validSamples++;
  }
  if (validSamples == 0) return false;

  // Expected count per bin: pdf evaluated at bin centre × bin solid angle.
  // Raw expected sums to validSamples × P(accept), which can be < validSamples
  // when GGX NDF sampling rejects below-horizon reflections. Renormalize so
  // Σ expected = Σ observed — the test then checks distribution shape only.
  for (int ti = 0; ti < BT; ti++) {
    float cosLo  = (float)ti / BT;
    float cosHi  = (float)(ti + 1) / BT;
    float cosMid = 0.5f * (cosLo + cosHi);
    float sinMid = sqrtf(std::max(0.0f, 1.0f - cosMid * cosMid));
    float dCos   = cosHi - cosLo;
    float dPhi   = 2.0f * M_PI / BP;
    for (int pi = 0; pi < BP; pi++) {
      float phi = (pi + 0.5f) / BP * 2.0f * M_PI;
      Vec3 wiLocal(sinMid * cosf(phi), sinMid * sinf(phi), cosMid);
      Vec3 wiWorld = sd.frame.toWorld(wiLocal);
      expected[ti * BP + pi] = bsdf->PDF(sd, wiWorld) * dCos * dPhi;
    }
  }
  float totalExpected = 0.0f;
  for (int i = 0; i < BINS; i++) totalExpected += expected[i];
  if (totalExpected <= 0.0f) return false;
  float norm = (float)validSamples / totalExpected;
  for (int i = 0; i < BINS; i++) expected[i] *= norm;

  // Chi-square statistic; skip bins with expected < 5 (standard rule)
  float chi2 = 0.0f;
  int dof = 0;
  for (int i = 0; i < BINS; i++) {
    if (expected[i] < 5.0f) continue;
    float d = (float)observed[i] - expected[i];
    chi2 += d * d / expected[i];
    dof++;
  }
  if (dof == 0) return false;

  // 5σ threshold above the chi-square mean to avoid false positives
  float threshold = (float)dof + 5.0f * sqrtf(2.0f * (float)dof);
  return chi2 < threshold;
}
