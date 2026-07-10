#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "sampling.h"

static constexpr int N = 1000;
static constexpr float MARGIN = 0.05f;

// -----------------------------------------------------------------------
// MTRandom
// -----------------------------------------------------------------------

TEST_CASE("MTRandom values are in [0, 1)") {
  MTRandom rng(42);
  for (int i = 0; i < N; i++) {
    float v = rng.next();
    REQUIRE(v >= 0.0f);
    REQUIRE(v < 1.0f);
  }
}

TEST_CASE("MTRandom is deterministic with same seed") {
  MTRandom a(123), b(123);
  for (int i = 0; i < 20; i++)
    REQUIRE(a.next() == b.next());
}

TEST_CASE("MTRandom different seeds produce different sequences") {
  MTRandom a(1), b(2);
  bool anyDiff = false;
  for (int i = 0; i < 20; i++)
    if (a.next() != b.next()) { anyDiff = true; break; }
  REQUIRE(anyDiff);
}

TEST_CASE("MTRandom average converges to 0.5") {
  MTRandom rng(42);
  float sum = 0.0f;
  for (int i = 0; i < N; i++) sum += rng.next();
  REQUIRE(sum / N == Catch::Approx(0.5f).margin(MARGIN));
}

// -----------------------------------------------------------------------
// uniformSampleHemisphere
// -----------------------------------------------------------------------

TEST_CASE("uniformSampleHemisphere produces unit vectors in upper hemisphere") {
  MTRandom rng(42);
  for (int i = 0; i < N; i++) {
    Vec3 s = SamplingDistributions::uniformSampleHemisphere(rng.next(), rng.next());
    REQUIRE(s.z >= 0.0f);
    REQUIRE(s.length() == Catch::Approx(1.0f).margin(1e-4f));
  }
}

TEST_CASE("uniformSampleHemisphere E[z] converges to 0.5") {
  // For uniform hemisphere: E[cos(theta)] = integral of z * (1/2PI) dw = 0.5
  MTRandom rng(42);
  float sum = 0.0f;
  for (int i = 0; i < N; i++)
    sum += SamplingDistributions::uniformSampleHemisphere(rng.next(), rng.next()).z;
  REQUIRE(sum / N == Catch::Approx(0.5f).margin(MARGIN));
}

// -----------------------------------------------------------------------
// uniformHemispherePDF
// -----------------------------------------------------------------------

TEST_CASE("uniformHemispherePDF is constant 1/(2*PI)") {
  float expected = 1.0f / (2.0f * PI);
  REQUIRE(SamplingDistributions::uniformHemispherePDF(Vec3(0.0f, 0.0f, 1.0f)) == Catch::Approx(expected));
  REQUIRE(SamplingDistributions::uniformHemispherePDF(Vec3(1.0f, 0.0f, 0.0f)) == Catch::Approx(expected));
  REQUIRE(SamplingDistributions::uniformHemispherePDF(Vec3(0.577f, 0.577f, 0.577f)) == Catch::Approx(expected));
}

// -----------------------------------------------------------------------
// cosineSampleHemisphere
// -----------------------------------------------------------------------

TEST_CASE("cosineSampleHemisphere produces unit vectors in upper hemisphere") {
  MTRandom rng(42);
  for (int i = 0; i < N; i++) {
    Vec3 s = SamplingDistributions::cosineSampleHemisphere(rng.next(), rng.next());
    REQUIRE(s.z >= 0.0f);
    REQUIRE(s.length() == Catch::Approx(1.0f).margin(1e-4f));
  }
}

TEST_CASE("cosineSampleHemisphere E[z] converges to 2/3") {
  // For cosine-weighted hemisphere: E[z] = integral of z * (z/PI) dw = 2/3
  MTRandom rng(42);
  float sum = 0.0f;
  for (int i = 0; i < N; i++)
    sum += SamplingDistributions::cosineSampleHemisphere(rng.next(), rng.next()).z;
  REQUIRE(sum / N == Catch::Approx(2.0f / 3.0f).margin(MARGIN));
}

// -----------------------------------------------------------------------
// cosineSampleHemisphereByDisk
// -----------------------------------------------------------------------

TEST_CASE("cosineSampleHemisphereByDisk produces unit vectors in upper hemisphere") {
  MTRandom rng(42);
  for (int i = 0; i < N; i++) {
    Vec3 s = SamplingDistributions::cosineSampleHemisphereByDisk(rng.next(), rng.next());
    REQUIRE(s.z >= 0.0f);
    REQUIRE(s.length() == Catch::Approx(1.0f).margin(1e-4f));
  }
}

TEST_CASE("cosineSampleHemisphereByDisk E[z] converges to 2/3") {
  MTRandom rng(42);
  float sum = 0.0f;
  for (int i = 0; i < N; i++)
    sum += SamplingDistributions::cosineSampleHemisphereByDisk(rng.next(), rng.next()).z;
  REQUIRE(sum / N == Catch::Approx(2.0f / 3.0f).margin(MARGIN));
}

// -----------------------------------------------------------------------
// cosineHemispherePDF
// -----------------------------------------------------------------------

TEST_CASE("cosineHemispherePDF at zenith equals 1/PI") {
  REQUIRE(SamplingDistributions::cosineHemispherePDF(Vec3(0.0f, 0.0f, 1.0f)) ==
          Catch::Approx(1.0f / PI).margin(1e-4f));
}

TEST_CASE("cosineHemispherePDF scales with z") {
  float pdf = SamplingDistributions::cosineHemispherePDF(Vec3(0.0f, 0.0f, 0.5f));
  REQUIRE(pdf == Catch::Approx(0.5f / PI).margin(1e-4f));
}

TEST_CASE("cosineHemispherePDF returns 0 for below-hemisphere direction") {
  REQUIRE(SamplingDistributions::cosineHemispherePDF(Vec3(0.0f, 0.0f, -1.0f)) == 0.0f);
  REQUIRE(SamplingDistributions::cosineHemispherePDF(Vec3(0.0f, 0.0f, -0.5f)) == 0.0f);
}

// -----------------------------------------------------------------------
// uniformSampleSphere
// -----------------------------------------------------------------------

TEST_CASE("uniformSampleSphere produces unit vectors") {
  MTRandom rng(42);
  for (int i = 0; i < N; i++) {
    Vec3 s = SamplingDistributions::uniformSampleSphere(rng.next(), rng.next());
    REQUIRE(s.length() == Catch::Approx(1.0f).margin(1e-4f));
  }
}

TEST_CASE("uniformSampleSphere E[z] converges to 0 (symmetric)") {
  MTRandom rng(42);
  float sum = 0.0f;
  for (int i = 0; i < N; i++)
    sum += SamplingDistributions::uniformSampleSphere(rng.next(), rng.next()).z;
  REQUIRE(sum / N == Catch::Approx(0.0f).margin(MARGIN));
}

// -----------------------------------------------------------------------
// uniformSpherePDF
// -----------------------------------------------------------------------

TEST_CASE("uniformSpherePDF is constant 1/(4*PI)") {
  float expected = 1.0f / (4.0f * PI);
  REQUIRE(SamplingDistributions::uniformSpherePDF(Vec3(0.0f, 0.0f,  1.0f)) == Catch::Approx(expected));
  REQUIRE(SamplingDistributions::uniformSpherePDF(Vec3(0.0f, 0.0f, -1.0f)) == Catch::Approx(expected));
  REQUIRE(SamplingDistributions::uniformSpherePDF(Vec3(1.0f, 0.0f,  0.0f)) == Catch::Approx(expected));
}

// -----------------------------------------------------------------------
// Monte Carlo integration — PDF correctness
// -----------------------------------------------------------------------

TEST_CASE("MC integration: uniform hemisphere integral of cos(theta) dw == PI") {
  // integral of cos(theta) over hemisphere = PI
  // MC estimate: (1/N) * sum( z_i / uniformHemispherePDF ) = 2*PI * E[z] = PI
  MTRandom rng(42);
  float sum = 0.0f;
  float pdf = SamplingDistributions::uniformHemispherePDF(Vec3(0, 0, 1));
  for (int i = 0; i < N; i++) {
    Vec3 s = SamplingDistributions::uniformSampleHemisphere(rng.next(), rng.next());
    sum += s.z / pdf;
  }
  REQUIRE(sum / N == Catch::Approx(PI).margin(0.1f));
}

TEST_CASE("MC integration: cosine hemisphere integral of 1 dw == 2*PI") {
  // integral of 1 over hemisphere = 2*PI
  // MC estimate with cosine samples: (1/N) * sum( 1 / cosineHemispherePDF(s_i) ) = (PI/N) * sum(1/z_i)
  // But better: integral of cos(theta)/PI dw = 1, so cosine PDF integrates to 1.
  // Equivalently: uniform hemisphere estimate of cosine PDF = 1:
  // (2*PI / N) * sum( cosineHemispherePDF(s_i) ) should == 1
  MTRandom rng(42);
  float sum = 0.0f;
  for (int i = 0; i < N; i++) {
    Vec3 s = SamplingDistributions::uniformSampleHemisphere(rng.next(), rng.next());
    sum += SamplingDistributions::cosineHemispherePDF(s);
  }
  REQUIRE((2.0f * PI * sum / N) == Catch::Approx(1.0f).margin(0.1f));
}
