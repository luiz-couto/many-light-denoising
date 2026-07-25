#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "shading.h"
#include <cmath>

// -----------------------------------------------------------------------
// getCosThetaT
// -----------------------------------------------------------------------

TEST_CASE("getCosThetaT: normal incidence, no TIR") {
  // cosTheta=1 (normal incidence), sin²=0 → cosThetaT = 1 regardless of n
  bool tir = true;
  float result = ShadingHelper::getCosThetaT(1.0f, 1.5f, tir);
  REQUIRE(tir == false);
  REQUIRE(result == Catch::Approx(1.0f));
}

TEST_CASE("getCosThetaT: Snell's law — 45 degree incidence into glass") {
  // n = intIOR/extIOR = 1.5/1.0 = 1.5
  // cosTheta = cos(45°) ≈ 0.7071
  // sin²ThetaT = sin²ThetaI / n² = 0.5 / 2.25 = 0.2222
  // cosThetaT = sqrt(1 - 0.2222) = sqrt(0.7778) ≈ 0.8819
  bool tir = true;
  float cosTheta = sqrtf(0.5f);
  float result = ShadingHelper::getCosThetaT(cosTheta, 1.5f, tir);
  REQUIRE(tir == false);
  REQUIRE(result == Catch::Approx(sqrtf(0.7778f)).margin(0.001f));
}

TEST_CASE("getCosThetaT: total internal reflection sets flag and returns 0") {
  // Going from glass (n=1.5) to air (n=1.0): n = extIOR/intIOR = 1.0/1.5 = 0.667
  // Critical angle: sinThetaC = 1/1.5 = 0.667, cosThetaC = sqrt(1-0.444) = 0.745
  // Below critical angle: cosTheta = 0.5 → sin²=0.75, sin²T = 0.75/0.444 = 1.69 > 1 → TIR
  bool tir = false;
  float result = ShadingHelper::getCosThetaT(0.5f, 1.0f / 1.5f, tir);
  REQUIRE(tir == true);
  REQUIRE(result == Catch::Approx(0.0f));
}

TEST_CASE("getCosThetaT: same medium (n=1) gives same angle back") {
  bool tir = true;
  float cosTheta = 0.8f;
  float result = ShadingHelper::getCosThetaT(cosTheta, 1.0f, tir);
  REQUIRE(tir == false);
  REQUIRE(result == Catch::Approx(cosTheta));
}

// -----------------------------------------------------------------------
// fresnelDielectric (two-IOR overload)
// -----------------------------------------------------------------------

TEST_CASE("fresnelDielectric: normal incidence into glass gives ~0.04") {
  // R = ((n-1)/(n+1))^2 = (0.5/2.5)^2 = 0.04
  float result = ShadingHelper::fresnelDielectric(1.0f, 1.5f, 1.0f);
  REQUIRE(result == Catch::Approx(0.04f).margin(0.001f));
}

TEST_CASE("fresnelDielectric: grazing incidence gives 1.0") {
  float result = ShadingHelper::fresnelDielectric(0.0f, 1.5f, 1.0f);
  REQUIRE(result == Catch::Approx(1.0f).margin(0.001f));
}

TEST_CASE("fresnelDielectric: same IOR gives 0 (no boundary)") {
  float result = ShadingHelper::fresnelDielectric(1.0f, 1.0f, 1.0f);
  REQUIRE(result == Catch::Approx(0.0f).margin(0.001f));
}

TEST_CASE("fresnelDielectric: total internal reflection returns 1.0") {
  // cosTheta negative = exiting glass; cosTheta=-0.5 is below critical angle
  float result = ShadingHelper::fresnelDielectric(-0.5f, 1.5f, 1.0f);
  REQUIRE(result == Catch::Approx(1.0f));
}

TEST_CASE("fresnelDielectric: result is in [0, 1]") {
  for (int i = 0; i <= 10; i++) {
    float cosTheta = i / 10.0f;
    float result = ShadingHelper::fresnelDielectric(cosTheta, 1.5f, 1.0f);
    REQUIRE(result >= 0.0f);
    REQUIRE(result <= 1.0f);
  }
}

TEST_CASE("fresnelDielectric: reciprocal — Helmholtz reciprocity along reversed ray") {
  // Entering air→glass at cosθ_i and exiting glass→air at the Snell-law transmitted angle
  // must give the same reflectance (reversing the ray doesn't change R).
  float cosThetaI = 0.8f;
  float entering = ShadingHelper::fresnelDielectric(cosThetaI, 1.5f, 1.0f);
  bool tir;
  float cosThetaT = ShadingHelper::getCosThetaT(cosThetaI, 1.5f, tir); // n = intIOR/extIOR = 1.5
  float exiting   = ShadingHelper::fresnelDielectric(-cosThetaT, 1.5f, 1.0f);
  REQUIRE(entering == Catch::Approx(exiting).margin(0.001f));
}

TEST_CASE("fresnelDielectric: single-n overload matches two-IOR overload (entering)") {
  // fresnelDielectric(cosTheta, n) with n = intIOR/extIOR must agree with the two-IOR version
  float cosTheta = 0.8f;
  float twoIOR  = ShadingHelper::fresnelDielectric(cosTheta, 1.5f, 1.0f);
  float singleN = ShadingHelper::fresnelDielectric(cosTheta, 1.5f);
  REQUIRE(twoIOR == Catch::Approx(singleN).margin(0.001f));
}

// -----------------------------------------------------------------------
// fresnelConductor
// -----------------------------------------------------------------------

TEST_CASE("fresnelConductor: result is in [0, 1] per channel") {
  // Gold-like IOR (n≈0.37, k≈2.82)
  Colour ior(0.37f, 0.37f, 0.37f);
  Colour k(2.82f, 2.82f, 2.82f);
  for (int i = 1; i <= 10; i++) {
    float cosTheta = i / 10.0f;
    Colour result = ShadingHelper::fresnelConductor(cosTheta, ior, k);
    REQUIRE(result.r >= 0.0f); REQUIRE(result.r <= 1.0f);
    REQUIRE(result.g >= 0.0f); REQUIRE(result.g <= 1.0f);
    REQUIRE(result.b >= 0.0f); REQUIRE(result.b <= 1.0f);
  }
}

TEST_CASE("fresnelConductor: perfect mirror (k→∞) gives reflectance near 1") {
  Colour ior(1.0f, 1.0f, 1.0f);
  Colour k(1000.0f, 1000.0f, 1000.0f);
  Colour result = ShadingHelper::fresnelConductor(1.0f, ior, k);
  REQUIRE(result.r == Catch::Approx(1.0f).margin(0.001f));
}

TEST_CASE("fresnelConductor: k=0 (lossless) matches dielectric at normal incidence") {
  // k=0 collapses conductor Fresnel to dielectric formula
  // For n=1.5, k=0: R = ((1.5-1)/(1.5+1))^2 = 0.04
  Colour ior(1.5f, 1.5f, 1.5f);
  Colour k(0.0f, 0.0f, 0.0f);
  Colour result = ShadingHelper::fresnelConductor(1.0f, ior, k);
  REQUIRE(result.r == Catch::Approx(0.04f).margin(0.002f));
}

// -----------------------------------------------------------------------
// fresnelConductorPerpendicularSqr — direct tests
// -----------------------------------------------------------------------

TEST_CASE("fresnelConductorPerpendicularSqr: normal incidence k=0 matches dielectric") {
  // At cosθ=1, k=0: ((n-1)/(n+1))^2 = 0.04 for n=1.5
  float result = ShadingHelper::fresnelConductorPerpendicularSqr(1.0f, 1.5f, 0.0f);
  REQUIRE(result == Catch::Approx(0.04f).margin(0.001f));
}

TEST_CASE("fresnelConductorPerpendicularSqr: result is in [0, 1]") {
  for (int i = 1; i <= 10; i++) {
    float cosTheta = i / 10.0f;
    float result = ShadingHelper::fresnelConductorPerpendicularSqr(cosTheta, 0.37f, 2.82f);
    REQUIRE(result >= 0.0f);
    REQUIRE(result <= 1.0f);
  }
}

TEST_CASE("fresnelConductorPerpendicularSqr: grazing incidence gives 1") {
  float result = ShadingHelper::fresnelConductorPerpendicularSqr(0.0f, 0.37f, 2.82f);
  REQUIRE(result == Catch::Approx(1.0f).margin(0.001f));
}

// -----------------------------------------------------------------------
// fresnelConductorParallelSqr — direct tests
// -----------------------------------------------------------------------

TEST_CASE("fresnelConductorParallelSqr: equals perpendicularSqr at normal incidence") {
  // At cosθ=1 the two polarisations are indistinguishable so Rs = Rp
  float n = 0.37f, k = 2.82f;
  float perp = ShadingHelper::fresnelConductorPerpendicularSqr(1.0f, n, k);
  float para = ShadingHelper::fresnelConductorParallelSqr(1.0f, n, k);
  REQUIRE(perp == Catch::Approx(para).margin(0.001f));
}

TEST_CASE("fresnelConductorParallelSqr: normal incidence k=0 matches dielectric") {
  float result = ShadingHelper::fresnelConductorParallelSqr(1.0f, 1.5f, 0.0f);
  REQUIRE(result == Catch::Approx(0.04f).margin(0.001f));
}

TEST_CASE("fresnelConductorParallelSqr: result is in [0, 1]") {
  for (int i = 1; i <= 10; i++) {
    float cosTheta = i / 10.0f;
    float result = ShadingHelper::fresnelConductorParallelSqr(cosTheta, 0.37f, 2.82f);
    REQUIRE(result >= 0.0f);
    REQUIRE(result <= 1.0f);
  }
}

TEST_CASE("fresnelConductorParallelSqr: grazing incidence gives 1") {
  float result = ShadingHelper::fresnelConductorParallelSqr(0.0f, 0.37f, 2.82f);
  REQUIRE(result == Catch::Approx(1.0f).margin(0.001f));
}

// -----------------------------------------------------------------------
// GGX — Dggx, lambdaGGX, Gggx
// -----------------------------------------------------------------------

TEST_CASE("Dggx: at normal incidence h=(0,0,1), D = alpha^2 / pi") {
  // When h = (0,0,1): cosThetaM = 1, term = alpha^2 - 1 + 1 = alpha^2
  // D = alpha^2 / (pi * alpha^4) = 1/pi/alpha^2... wait:
  // term = cosThetaM^2 * (alpha^2 - 1) + 1 = 1*(alpha^2-1)+1 = alpha^2
  // D = alpha^2 / (pi * (alpha^2)^2) = 1 / (pi * alpha^2)
  float alpha = 0.5f;
  Vec3 h(0.0f, 0.0f, 1.0f);
  float result = ShadingHelper::Dggx(h, alpha);
  float expected = 1.0f / (M_PI * alpha * alpha);
  REQUIRE(result == Catch::Approx(expected).margin(0.001f));
}

TEST_CASE("Dggx: alpha=1 (maximally rough), h=(0,0,1) gives 1/pi") {
  Vec3 h(0.0f, 0.0f, 1.0f);
  float result = ShadingHelper::Dggx(h, 1.0f);
  REQUIRE(result == Catch::Approx(1.0f / M_PI).margin(0.001f));
}

TEST_CASE("Dggx: result is non-negative") {
  Vec3 h(0.5f, 0.0f, sqrtf(0.75f)); // some off-normal direction
  REQUIRE(ShadingHelper::Dggx(h, 0.3f) >= 0.0f);
  REQUIRE(ShadingHelper::Dggx(h, 0.8f) >= 0.0f);
}

TEST_CASE("lambdaGGX: at normal incidence wi=(0,0,1) gives 0") {
  // cosTheta=1, tanTheta=0 → term=1, lambda=(1-1)/2 = 0
  Vec3 wi(0.0f, 0.0f, 1.0f);
  REQUIRE(ShadingHelper::lambdaGGX(wi, 0.5f) == Catch::Approx(0.0f));
}

TEST_CASE("lambdaGGX: result is non-negative") {
  Vec3 wi(0.5f, 0.0f, sqrtf(0.75f));
  REQUIRE(ShadingHelper::lambdaGGX(wi, 0.3f) >= 0.0f);
  REQUIRE(ShadingHelper::lambdaGGX(wi, 0.8f) >= 0.0f);
}

TEST_CASE("Gggx: at normal incidence wi=wo=(0,0,1) gives 1") {
  // lambda=0 for both → G = (1/(1+0)) * (1/(1+0)) = 1
  Vec3 wi(0.0f, 0.0f, 1.0f);
  Vec3 wo(0.0f, 0.0f, 1.0f);
  REQUIRE(ShadingHelper::Gggx(wi, wo, 0.5f) == Catch::Approx(1.0f));
}

TEST_CASE("Gggx: result is in [0, 1]") {
  Vec3 wi(0.5f, 0.0f, sqrtf(0.75f));
  Vec3 wo(0.3f, 0.0f, sqrtf(0.91f));
  float result = ShadingHelper::Gggx(wi, wo, 0.5f);
  REQUIRE(result >= 0.0f);
  REQUIRE(result <= 1.0f);
}
