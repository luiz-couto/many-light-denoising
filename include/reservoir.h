#ifndef RESERVOIR_H
#define RESERVOIR_H

struct Reservoir {
  int vplIndex = -1;
  float weightSum = 0.0f;
  float candidateCount = 0.0f;
  float contributionWeight = 0.0f;

  bool update(int candidateIndex, float weight, float random);
  bool merge(const Reservoir& other, float pHatAtMine, float random);
  void finalize(float pHatOfWinner);
};

#endif // RESERVOIR_H
