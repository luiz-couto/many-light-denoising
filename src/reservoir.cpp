#include "reservoir.h"

bool Reservoir::update(int candidateIndex, float weight, float random) {
  weightSum += weight;
  candidateCount++;

  if (weight > 0.0f && random < weight / weightSum) {
    vplIndex = candidateIndex;
    return true;
  }

  return false;
}

bool Reservoir::merge(const Reservoir& other, float pHatAtMine, float random) {
  float weight = pHatAtMine * other.contributionWeight * other.candidateCount;
  weightSum += weight;
  candidateCount += other.candidateCount;

  if (weight > 0.0f && random < weight / weightSum) {
    vplIndex = other.vplIndex;
    return true;
  }

  return false;
}

void Reservoir::finalize(float pHatOfWinner) {
  if (vplIndex >= 0 && pHatOfWinner > 0.0f && candidateCount > 0.0f) {
    contributionWeight = weightSum / (candidateCount * pHatOfWinner);
    return;
  }

  contributionWeight = 0.0f;
}
