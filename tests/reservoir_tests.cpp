#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
#include "reservoir.h"
#include "sampling.h"

// -----------------------------------------------------------------------
// Contract these tests encode:
//
//   update(index, weight, random):
//     weightSum += weight; candidateCount += 1;
//     seat changes iff weight > 0 and random < weight/weightSum (strict <).
//     Zero-weight candidates COUNT in candidateCount but can never win
//     (same rule as estimateReflectance dividing by all draws).
//
//   merge(other, pHatAtMine, random):
//     super-candidate weight = pHatAtMine * other.contributionWeight
//                              * other.candidateCount;
//     candidateCount += other.candidateCount (absorb their audition).
//
//   finalize(pHatOfWinner):
//     contributionWeight = weightSum / (candidateCount * pHatOfWinner),
//     or 0 when the reservoir is empty, pHatOfWinner <= 0, or count == 0.
// -----------------------------------------------------------------------

// -----------------------------------------------------------------------
// update — deterministic mechanics
// -----------------------------------------------------------------------

TEST_CASE("Reservoir default state: empty, all zeros") {
  Reservoir reservoir;
  REQUIRE(reservoir.vplIndex == -1);
  REQUIRE(reservoir.weightSum == 0.0f);
  REQUIRE(reservoir.candidateCount == 0.0f);
  REQUIRE(reservoir.contributionWeight == 0.0f);
}

TEST_CASE("Reservoir update: first positive candidate always takes the seat") {
  // First candidate: weight/weightSum = 1.0, so even random = 0.999 admits it.
  Reservoir reservoir;
  bool took = reservoir.update(7, 2.0f, 0.999f);
  REQUIRE(took);
  REQUIRE(reservoir.vplIndex == 7);
  REQUIRE(reservoir.weightSum == Catch::Approx(2.0f));
  REQUIRE(reservoir.candidateCount == Catch::Approx(1.0f));
}

TEST_CASE("Reservoir update: replacement probability is weight/weightSum (strict <)") {
  // A (w=2) seated; B (w=3) challenges: threshold = 3/5 = 0.6.
  Reservoir justBelow;
  justBelow.update(0, 2.0f, 0.0f);
  REQUIRE(justBelow.update(1, 3.0f, 0.599f));   // random < 0.6 -> B usurps
  REQUIRE(justBelow.vplIndex == 1);

  Reservoir atThreshold;
  atThreshold.update(0, 2.0f, 0.0f);
  REQUIRE_FALSE(atThreshold.update(1, 3.0f, 0.6f));  // random == 0.6 -> strict <, A keeps
  REQUIRE(atThreshold.vplIndex == 0);

  // Bookkeeping identical either way.
  REQUIRE(atThreshold.weightSum == Catch::Approx(5.0f));
  REQUIRE(atThreshold.candidateCount == Catch::Approx(2.0f));
}

TEST_CASE("Reservoir update: zero-weight candidate counts in M but can never win") {
  Reservoir reservoir;
  reservoir.update(0, 4.0f, 0.0f);
  bool took = reservoir.update(1, 0.0f, 0.0f);  // random 0.0 would win anything winnable
  REQUIRE_FALSE(took);
  REQUIRE(reservoir.vplIndex == 0);
  REQUIRE(reservoir.weightSum == Catch::Approx(4.0f));       // unchanged
  REQUIRE(reservoir.candidateCount == Catch::Approx(2.0f));  // still counted
}

TEST_CASE("Reservoir update: all-zero stream stays empty, no NaN") {
  // First candidates all score zero: weight/weightSum would be 0/0 without
  // the weight > 0 guard. Must stay empty and finite.
  Reservoir reservoir;
  for (int i = 0; i < 5; i++) {
    REQUIRE_FALSE(reservoir.update(i, 0.0f, 0.0f));
  }
  REQUIRE(reservoir.vplIndex == -1);
  REQUIRE(reservoir.weightSum == 0.0f);
  REQUIRE(reservoir.candidateCount == Catch::Approx(5.0f));
  reservoir.finalize(1.0f);
  REQUIRE(reservoir.contributionWeight == 0.0f);
  REQUIRE(std::isfinite(reservoir.contributionWeight));
}

// -----------------------------------------------------------------------
// update — the telescoping distribution
// -----------------------------------------------------------------------

TEST_CASE("Reservoir update: final winner distribution equals weight shares (2/3/5)") {
  // Streaming candidates with weights 2, 3, 5 must seat them with final
  // probabilities 0.2 / 0.3 / 0.5 — the telescoping-product property.
  MTRandom sampler(42);
  const int trials = 100000;
  int wins[3] = {0, 0, 0};

  for (int t = 0; t < trials; t++) {
    Reservoir reservoir;
    reservoir.update(0, 2.0f, sampler.next());
    reservoir.update(1, 3.0f, sampler.next());
    reservoir.update(2, 5.0f, sampler.next());
    wins[reservoir.vplIndex]++;
  }

  REQUIRE((float)wins[0] / trials == Catch::Approx(0.2f).margin(0.01f));
  REQUIRE((float)wins[1] / trials == Catch::Approx(0.3f).margin(0.01f));
  REQUIRE((float)wins[2] / trials == Catch::Approx(0.5f).margin(0.01f));
}

// -----------------------------------------------------------------------
// finalize
// -----------------------------------------------------------------------

TEST_CASE("Reservoir finalize: contributionWeight = weightSum / (count * pHatOfWinner)") {
  // Candidates with weights 8 then 4: second's threshold = 4/12 = 1/3;
  // random 0.5 keeps the first seated. weightSum = 12, count = 2.
  Reservoir reservoir;
  reservoir.update(3, 8.0f, 0.0f);
  reservoir.update(9, 4.0f, 0.5f);
  REQUIRE(reservoir.vplIndex == 3);

  reservoir.finalize(2.0f);   // pHat of winner = 2
  REQUIRE(reservoir.contributionWeight == Catch::Approx(12.0f / (2.0f * 2.0f)));  // = 3
}

TEST_CASE("Reservoir finalize: uniform scores collapse to W = N (naive-estimator anchor)") {
  // With q = 1/N, candidate weight = pHat * N. If every candidate has the
  // SAME pHat, then W = (M * pHat * N) / (M * pHat) = N — ReSTIR degrades
  // exactly to 'pick one of N, multiply by N'. Independent of the randoms.
  const float N = 200.0f;
  const float pHatValue = 0.7f;
  MTRandom sampler(7);

  Reservoir reservoir;
  for (int i = 0; i < 4; i++) {
    reservoir.update(i, pHatValue * N, sampler.next());
  }
  reservoir.finalize(pHatValue);
  REQUIRE(reservoir.contributionWeight == Catch::Approx(N).epsilon(1e-4f));
}

TEST_CASE("Reservoir finalize: guards — empty reservoir and zero pHat give W = 0") {
  Reservoir empty;
  empty.finalize(1.0f);
  REQUIRE(empty.contributionWeight == 0.0f);

  Reservoir seated;
  seated.update(0, 5.0f, 0.0f);
  seated.finalize(0.0f);        // winner scores zero at finalize time
  REQUIRE(seated.contributionWeight == 0.0f);
  REQUIRE(std::isfinite(seated.contributionWeight));
}

// -----------------------------------------------------------------------
// merge — deterministic mechanics
// -----------------------------------------------------------------------

TEST_CASE("Reservoir merge: super-candidate weight and head-count absorption") {
  // Mine: one candidate, weight 6.
  Reservoir mine;
  mine.update(0, 6.0f, 0.0f);

  // Theirs: finalized with W = 2, M = 3, winner index 5.
  Reservoir theirs;
  theirs.vplIndex = 5;
  theirs.contributionWeight = 2.0f;
  theirs.candidateCount = 3.0f;

  // Super-candidate weight = pHatAtMine * W * M = 1.5 * 2 * 3 = 9.
  // Threshold = 9 / (6 + 9) = 0.6.
  bool took = mine.merge(theirs, 1.5f, 0.599f);
  REQUIRE(took);
  REQUIRE(mine.vplIndex == 5);
  REQUIRE(mine.weightSum == Catch::Approx(15.0f));
  REQUIRE(mine.candidateCount == Catch::Approx(4.0f));  // 1 + 3: absorbed their audition

  // Same setup, random at the threshold: strict <, keeps own winner.
  Reservoir mine2;
  mine2.update(0, 6.0f, 0.0f);
  REQUIRE_FALSE(mine2.merge(theirs, 1.5f, 0.6f));
  REQUIRE(mine2.vplIndex == 0);
  REQUIRE(mine2.candidateCount == Catch::Approx(4.0f)); // bookkeeping identical either way
}

TEST_CASE("Reservoir merge: empty other is a no-op on the seat and adds no weight") {
  Reservoir mine;
  mine.update(0, 6.0f, 0.0f);

  Reservoir empty;  // vplIndex -1, W 0, M 0
  bool took = mine.merge(empty, 1.0f, 0.0f);
  REQUIRE_FALSE(took);
  REQUIRE(mine.vplIndex == 0);
  REQUIRE(mine.weightSum == Catch::Approx(6.0f));
  // NOTE: callers skip empty neighbours anyway (phase 2 convention), so
  // whether M grows here is moot for the renderer; this documents safety.
}

// -----------------------------------------------------------------------
// merge — the union property (the theorem merge() exists to satisfy)
// -----------------------------------------------------------------------

TEST_CASE("Reservoir merge: merging two half-streams matches streaming the union") {
  // One pixel, four lamps with pHat = {1, 2, 3, 4} (candidate weight = pHat;
  // the constant N/q factor cancels in the winner distribution).
  // Method A: stream all four into one reservoir.
  // Method B: stream {0,1} into R1, {2,3} into R2, finalize R2, merge into R1.
  // Both must seat winner j with probability pHat_j / 10.
  const float pHat[4] = {1.0f, 2.0f, 3.0f, 4.0f};
  const int trials = 100000;
  MTRandom sampler(1234);

  int winsStreamed[4] = {0, 0, 0, 0};
  int winsMerged[4]   = {0, 0, 0, 0};

  for (int t = 0; t < trials; t++) {
    Reservoir streamed;
    for (int j = 0; j < 4; j++) streamed.update(j, pHat[j], sampler.next());
    winsStreamed[streamed.vplIndex]++;

    Reservoir first;
    first.update(0, pHat[0], sampler.next());
    first.update(1, pHat[1], sampler.next());

    Reservoir second;
    second.update(2, pHat[2], sampler.next());
    second.update(3, pHat[3], sampler.next());
    second.finalize(pHat[second.vplIndex]);

    first.merge(second, pHat[second.vplIndex], sampler.next());
    winsMerged[first.vplIndex]++;
  }

  for (int j = 0; j < 4; j++) {
    float expected = pHat[j] / 10.0f;
    REQUIRE((float)winsStreamed[j] / trials == Catch::Approx(expected).margin(0.01f));
    REQUIRE((float)winsMerged[j] / trials   == Catch::Approx(expected).margin(0.01f));
  }
}
