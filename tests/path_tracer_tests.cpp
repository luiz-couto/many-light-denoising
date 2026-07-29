#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "path_tracer.h"
#include "scene.h"
#include "film.h"
#include "geometry.h"
#include "light.h"

static const Colour WHITE(1.0f, 1.0f, 1.0f);
static const Colour BLACK(0.0f, 0.0f, 0.0f);

// -----------------------------------------------------------------------
// Mocks
// -----------------------------------------------------------------------

class FixedBSDF : public BSDF {
public:
    Colour colour;
    explicit FixedBSDF(Colour c) : colour(c) {}
    Vec3   sample(const ShadingData&, Sampler*, Colour& w, float& pdf) override { w = colour; pdf = 1.0f; return Vec3(0.0f, 1.0f, 0.0f); }
    Colour evaluate(const ShadingData&, const Vec3&) override { return colour; }
    float  PDF(const ShadingData&, const Vec3&) override { return 1.0f; }
    bool   isPureSpecular() override { return false; }
    bool   isTwoSided() override { return true; }
    float  mask(const ShadingData&) override { return 1.0f; }
};

// sample() returns pdf=0 — triggers the pdf guard, no indirect contribution.
// Also makes the MIS weight 0 in lightSamplingMIS, so directLight == bsdf·L·cosTheta/lightPDF.
class ZeroPDFBSDF : public BSDF {
public:
    Colour colour;
    explicit ZeroPDFBSDF(Colour c) : colour(c) {}
    Vec3   sample(const ShadingData&, Sampler*, Colour& w, float& pdf) override { w = colour; pdf = 0.0f; return Vec3(0.0f, 1.0f, 0.0f); }
    Colour evaluate(const ShadingData&, const Vec3&) override { return colour; }
    float  PDF(const ShadingData&, const Vec3&) override { return 0.0f; }
    bool   isPureSpecular() override { return false; }
    bool   isTwoSided() override { return true; }
    float  mask(const ShadingData&) override { return 1.0f; }
};

// sample() returns a direction tangent to the surface — dot(worldDir, sNormal)=0,
// triggering the cosTheta < 1e-6 guard.
class TangentBSDF : public BSDF {
public:
    Vec3   sample(const ShadingData&, Sampler*, Colour& w, float& pdf) override { w = WHITE; pdf = 1.0f; return Vec3(1.0f, 0.0f, 0.0f); }
    Colour evaluate(const ShadingData&, const Vec3&) override { return WHITE; }
    float  PDF(const ShadingData&, const Vec3&) override { return 1.0f; }
    bool   isPureSpecular() override { return false; }
    bool   isTwoSided() override { return true; }
    float  mask(const ShadingData&) override { return 1.0f; }
};

// Env light that always returns a fixed direction, emission, and pdf without consuming
// any sampler values — makes tests fully deterministic.
class MockEnvLight : public Light {
public:
    Vec3   dir;
    Colour emission;
    float  pdfVal;
    MockEnvLight(Vec3 d, Colour e, float p) : dir(d.normalize()), emission(e), pdfVal(p) {}
    Vec3   sample(const ShadingData&, Sampler*, Colour& e, float& pdf) override { e = emission; pdf = pdfVal; return dir; }
    Colour evaluate(const Vec3&) override { return emission; }
    float  PDF(const ShadingData&, const Vec3&) override { return pdfVal; }
    bool   isArea() override { return false; }
    Vec3   normal(const ShadingData&, const Vec3&) override { return Vec3(0.0f, 1.0f, 0.0f); }
    float  totalIntegratedPower() override { return emission.lum() * 4.0f * (float)M_PI; }
    Vec3   samplePositionFromLight(Sampler*, float& pdf) override { pdf = 1.0f; return dir * 1000.0f; }
    Vec3   sampleDirectionFromLight(Sampler*, float& pdf) override { pdf = pdfVal; return dir; }
};

// Returns a fixed, wrapping sequence of floats — makes all sampler-dependent code deterministic.
class FixedSampler : public Sampler {
    std::vector<float> values;
    size_t idx = 0;
public:
    explicit FixedSampler(std::initializer_list<float> v) : values(v) {}
    float next() override { float v = values[idx % values.size()]; ++idx; return v; }
};

// -----------------------------------------------------------------------
// Geometry helpers
// -----------------------------------------------------------------------

// Emissive triangle at y=2, normal=(0,-1,0), area=0.5. materialIndex=0.
// A ray from (0.25,0,0.25) going up (0,1,0) hits this triangle at t=2.0.
static Triangle makeLightTriangle() {
    Vec3 down(0.0f, -1.0f, 0.0f);
    Vertex v0(Vec3(0.0f, 2.0f, 0.0f), down, 0.0f, 0.0f);
    Vertex v1(Vec3(1.0f, 2.0f, 0.0f), down, 1.0f, 0.0f);
    Vertex v2(Vec3(0.0f, 2.0f, 1.0f), down, 0.0f, 1.0f);
    Triangle t; t.init(v0, v1, v2, 0); return t;
}

// Second light triangle shifted +2 in X. Same geometry and area. materialIndex=0.
static Triangle makeSecondLightTriangle() {
    Vec3 down(0.0f, -1.0f, 0.0f);
    Vertex v0(Vec3(2.0f, 2.0f, 0.0f), down, 0.0f, 0.0f);
    Vertex v1(Vec3(3.0f, 2.0f, 0.0f), down, 1.0f, 0.0f);
    Vertex v2(Vec3(2.0f, 2.0f, 1.0f), down, 0.0f, 1.0f);
    Triangle t; t.init(v0, v1, v2, 0); return t;
}

// Large non-emissive floor at y=0, normal=(0,1,0). materialIndex=0.
// A ray from (0,2,0) going down (0,-1,0) hits this at t=2.0.
// FixedBSDF.sample() returns direction (0,1,0) — that ray misses the floor on recursion.
static Triangle makeFloorTriangle() {
    Vec3 up(0.0f, 1.0f, 0.0f);
    Vertex v0(Vec3(-10.0f, 0.0f, -10.0f), up, 0.0f, 0.0f);
    Vertex v1(Vec3( 10.0f, 0.0f, -10.0f), up, 1.0f, 0.0f);
    Vertex v2(Vec3(-10.0f, 0.0f,  10.0f), up, 0.0f, 1.0f);
    Triangle t; t.init(v0, v1, v2, 0); return t;
}

// Dummy triangle far below — keeps BVH non-empty without blocking upward rays.
static Triangle makeDistantTriangle() {
    Vec3 up(0.0f, 1.0f, 0.0f);
    Vertex v0(Vec3(-100.0f, -100.0f, -100.0f), up, 0.0f, 0.0f);
    Vertex v1(Vec3( 100.0f, -100.0f, -100.0f), up, 1.0f, 0.0f);
    Vertex v2(Vec3(-100.0f, -100.0f,  100.0f), up, 0.0f, 1.0f);
    Triangle t; t.init(v0, v1, v2, 0); return t;
}

// -----------------------------------------------------------------------
// areaLightSelectionPDF tests
//
// AreaLight::totalIntegratedPower = area * emission.lum() * π
// Single WHITE area light (area=0.5): power = 0.5π
// areaLightSelectionPDF = (power / totalPower) * (1 / area)
// -----------------------------------------------------------------------

TEST_CASE("areaLightSelectionPDF: single area light returns pmf * 1/area") {
    // One WHITE light, area=0.5. pmf=1 → result = 1.0 * (1/0.5) = 2.0
    FixedBSDF emissiveBSDF(WHITE); emissiveBSDF.emission = WHITE;
    BackgroundColour blackBg(BLACK);
    Triangle lightTri = makeLightTriangle();
    Scene scene; Film film;
    scene.init({lightTri}, {&emissiveBSDF}, &blackBg);
    scene.build();
    scene.width = 64; scene.height = 64; film.init(64, 64);

    REQUIRE(scene.areaLightSelectionPDF(0) == Catch::Approx(2.0f).margin(1e-5f));
}

TEST_CASE("areaLightSelectionPDF: two equal area lights give pmf=0.5 each") {
    // Equal power → pmf=0.5 each → areaLightSelectionPDF = 0.5 * 2.0 = 1.0 for each.
    FixedBSDF emissiveBSDF(WHITE); emissiveBSDF.emission = WHITE;
    BackgroundColour blackBg(BLACK);
    Triangle lightTri1 = makeLightTriangle();
    Triangle lightTri2 = makeSecondLightTriangle();
    Scene scene; Film film;
    scene.init({lightTri1, lightTri2}, {&emissiveBSDF}, &blackBg);
    scene.build();
    scene.width = 64; scene.height = 64; film.init(64, 64);

    REQUIRE(scene.areaLightSelectionPDF(0) == Catch::Approx(1.0f).margin(1e-5f));
    REQUIRE(scene.areaLightSelectionPDF(1) == Catch::Approx(1.0f).margin(1e-5f));
}

TEST_CASE("areaLightSelectionPDF: non-area-light triangle ID returns 0") {
    // triangles[0] = non-emissive floor, triangles[1] = area light.
    // areaLightSelectionPDF(0) must be 0 — no AreaLight covers triangles[0].
    // areaLightSelectionPDF(1) must be 2.0 — the only area light.
    FixedBSDF floorBSDF(BLACK);
    FixedBSDF emissiveBSDF(WHITE); emissiveBSDF.emission = WHITE;
    BackgroundColour blackBg(BLACK);
    Triangle floorTri = makeFloorTriangle(); floorTri.materialIndex = 0;
    Triangle lightTri = makeLightTriangle(); lightTri.materialIndex = 1;
    Scene scene; Film film;
    scene.init({floorTri, lightTri}, {&floorBSDF, &emissiveBSDF}, &blackBg);
    scene.build();
    scene.width = 64; scene.height = 64; film.init(64, 64);

    REQUIRE(scene.areaLightSelectionPDF(0) == Catch::Approx(0.0f).margin(1e-5f));
    REQUIRE(scene.areaLightSelectionPDF(1) == Catch::Approx(2.0f).margin(1e-5f));
}

TEST_CASE("areaLightSelectionPDF: no lights in scene returns 0") {
    // No emissive triangles, black background (totalPower=0) → totalPower guard fires.
    FixedBSDF dummyBSDF(BLACK);
    BackgroundColour blackBg(BLACK);
    Triangle dummy = makeDistantTriangle();
    Scene scene; Film film;
    scene.init({dummy}, {&dummyBSDF}, &blackBg);
    scene.build();
    scene.width = 64; scene.height = 64; film.init(64, 64);

    REQUIRE(scene.areaLightSelectionPDF(0) == Catch::Approx(0.0f).margin(1e-5f));
}

// -----------------------------------------------------------------------
// pathTrace: depth limit and ray miss
// -----------------------------------------------------------------------

TEST_CASE("pathTrace: depth > MAX_DEPTH returns black regardless of scene") {
    FixedBSDF emissiveBSDF(WHITE); emissiveBSDF.emission = WHITE;
    BackgroundColour blackBg(BLACK);
    Triangle lightTri = makeLightTriangle();
    Scene scene; Film film;
    scene.init({lightTri}, {&emissiveBSDF}, &blackBg);
    scene.build();
    scene.width = 64; scene.height = 64; film.init(64, 64);
    PathTracerIntegrator pt(&scene, &film);
    MTRandom sampler;

    Ray ray; ray.init(Vec3(0.25f, 0.0f, 0.25f), Vec3(0.0f, 1.0f, 0.0f));
    Colour result = pt.pathTrace(ray, WHITE, PathTracerIntegrator::MAX_DEPTH + 1, 0.0f, &sampler, false);

    REQUIRE(result.r == Catch::Approx(0.0f).margin(1e-5f));
    REQUIRE(result.g == Catch::Approx(0.0f).margin(1e-5f));
    REQUIRE(result.b == Catch::Approx(0.0f).margin(1e-5f));
}

TEST_CASE("pathTrace: depth == MAX_DEPTH still processes (boundary, not killed)") {
    // Only depth > MAX_DEPTH returns black — depth == MAX_DEPTH is still valid.
    // Glass TIR paths can reach MAX_DEPTH=5; this verifies they are not prematurely killed.
    // An emissive hit at depth=MAX_DEPTH with isSpecularBounce=true returns throughput*emission.
    FixedBSDF emissiveBSDF(WHITE); emissiveBSDF.emission = WHITE;
    BackgroundColour blackBg(BLACK);
    Triangle lightTri = makeLightTriangle();
    Scene scene; Film film;
    scene.init({lightTri}, {&emissiveBSDF}, &blackBg);
    scene.build();
    scene.width = 64; scene.height = 64; film.init(64, 64);
    PathTracerIntegrator pt(&scene, &film);
    MTRandom sampler;

    Ray ray; ray.init(Vec3(0.25f, 0.0f, 0.25f), Vec3(0.0f, 1.0f, 0.0f));
    Colour result = pt.pathTrace(ray, WHITE, PathTracerIntegrator::MAX_DEPTH, 1.0f, &sampler, true);

    REQUIRE(result.r == Catch::Approx(1.0f).margin(1e-5f));
    REQUIRE(result.g == Catch::Approx(1.0f).margin(1e-5f));
    REQUIRE(result.b == Catch::Approx(1.0f).margin(1e-5f));
}

TEST_CASE("pathTrace: ray miss returns throughput * background colour") {
    // Background = RED. Ray going up misses the distant triangle below.
    // Result = throughput * RED, not just RED — verifies the throughput multiply.
    FixedBSDF dummyBSDF(BLACK);
    BackgroundColour redBg(Colour(1.0f, 0.0f, 0.0f));
    Triangle dummy = makeDistantTriangle();
    Scene scene; Film film;
    scene.init({dummy}, {&dummyBSDF}, &redBg);
    scene.build();
    scene.width = 64; scene.height = 64; film.init(64, 64);
    PathTracerIntegrator pt(&scene, &film);
    MTRandom sampler;

    Ray ray; ray.init(Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f));
    Colour throughput(0.5f, 0.5f, 0.5f);
    Colour result = pt.pathTrace(ray, throughput, 0, 0.0f, &sampler, false);

    REQUIRE(result.r == Catch::Approx(0.5f).margin(1e-5f));
    REQUIRE(result.g == Catch::Approx(0.0f).margin(1e-5f));
    REQUIRE(result.b == Catch::Approx(0.0f).margin(1e-5f));
}

TEST_CASE("pathTrace: ray miss with BLACK background returns black") {
    FixedBSDF dummyBSDF(BLACK);
    BackgroundColour blackBg(BLACK);
    Triangle dummy = makeDistantTriangle();
    Scene scene; Film film;
    scene.init({dummy}, {&dummyBSDF}, &blackBg);
    scene.build();
    scene.width = 64; scene.height = 64; film.init(64, 64);
    PathTracerIntegrator pt(&scene, &film);
    MTRandom sampler;

    Ray ray; ray.init(Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f));
    Colour result = pt.pathTrace(ray, WHITE, 0, 0.0f, &sampler, false);

    REQUIRE(result.r == Catch::Approx(0.0f).margin(1e-5f));
    REQUIRE(result.g == Catch::Approx(0.0f).margin(1e-5f));
    REQUIRE(result.b == Catch::Approx(0.0f).margin(1e-5f));
}

// -----------------------------------------------------------------------
// pathTrace: emissive hit — full emission branch (depth=0 or specular bounce)
//
// Ray from (0.25,0,0.25) up (0,1,0) hits makeLightTriangle at t=2.
// Both the depth=0 and isSpecularBounce=true cases skip BSDF MIS and
// return throughput * emission with weight=1.
// -----------------------------------------------------------------------

TEST_CASE("pathTrace: emissive hit at depth=0 returns throughput * emission") {
    FixedBSDF emissiveBSDF(WHITE); emissiveBSDF.emission = WHITE;
    BackgroundColour blackBg(BLACK);
    Triangle lightTri = makeLightTriangle();
    Scene scene; Film film;
    scene.init({lightTri}, {&emissiveBSDF}, &blackBg);
    scene.build();
    scene.width = 64; scene.height = 64; film.init(64, 64);
    PathTracerIntegrator pt(&scene, &film);
    MTRandom sampler;

    Ray ray; ray.init(Vec3(0.25f, 0.0f, 0.25f), Vec3(0.0f, 1.0f, 0.0f));
    Colour throughput(0.4f, 0.6f, 0.8f);
    Colour result = pt.pathTrace(ray, throughput, 0, 0.0f, &sampler, false);

    // emission=WHITE → result = throughput * WHITE = throughput
    REQUIRE(result.r == Catch::Approx(0.4f).margin(1e-5f));
    REQUIRE(result.g == Catch::Approx(0.6f).margin(1e-5f));
    REQUIRE(result.b == Catch::Approx(0.8f).margin(1e-5f));
}

TEST_CASE("pathTrace: emissive hit with isSpecularBounce=true returns throughput * emission") {
    // Light sampling cannot reach a light via a specular reflection, so weight=1.
    FixedBSDF emissiveBSDF(WHITE); emissiveBSDF.emission = WHITE;
    BackgroundColour blackBg(BLACK);
    Triangle lightTri = makeLightTriangle();
    Scene scene; Film film;
    scene.init({lightTri}, {&emissiveBSDF}, &blackBg);
    scene.build();
    scene.width = 64; scene.height = 64; film.init(64, 64);
    PathTracerIntegrator pt(&scene, &film);
    MTRandom sampler;

    Ray ray; ray.init(Vec3(0.25f, 0.0f, 0.25f), Vec3(0.0f, 1.0f, 0.0f));
    Colour throughput(0.3f, 0.5f, 0.7f);
    Colour result = pt.pathTrace(ray, throughput, 2, 1.0f, &sampler, true);

    REQUIRE(result.r == Catch::Approx(0.3f).margin(1e-5f));
    REQUIRE(result.g == Catch::Approx(0.5f).margin(1e-5f));
    REQUIRE(result.b == Catch::Approx(0.7f).margin(1e-5f));
}

TEST_CASE("pathTrace: emissive hit at depth=0 with asymmetric throughput catches missing multiply") {
    // Distinct R/G/B throughput values: if throughput were missing, result = WHITE (1,1,1).
    FixedBSDF emissiveBSDF(WHITE); emissiveBSDF.emission = WHITE;
    BackgroundColour blackBg(BLACK);
    Triangle lightTri = makeLightTriangle();
    Scene scene; Film film;
    scene.init({lightTri}, {&emissiveBSDF}, &blackBg);
    scene.build();
    scene.width = 64; scene.height = 64; film.init(64, 64);
    PathTracerIntegrator pt(&scene, &film);
    MTRandom sampler;

    Ray ray; ray.init(Vec3(0.25f, 0.0f, 0.25f), Vec3(0.0f, 1.0f, 0.0f));
    Colour throughput(0.25f, 0.5f, 0.125f);
    Colour result = pt.pathTrace(ray, throughput, 0, 0.0f, &sampler, false);

    REQUIRE(result.r == Catch::Approx(0.25f).margin(1e-5f));
    REQUIRE(result.g == Catch::Approx(0.5f).margin(1e-5f));
    REQUIRE(result.b == Catch::Approx(0.125f).margin(1e-5f));
}

// -----------------------------------------------------------------------
// pathTrace: BSDF sampling MIS branch
//
// Single WHITE area light (area=0.5) at y=2. Ray from (0.25,0,0.25) → (0,1,0).
//   intersection.t = 2.0  →  distSq = 4.0
//   shadingData.sNormal = (0,-1,0),  shadingData.wo = (0,-1,0)
//   cosThetaLight = |sNormal · wo| = 1.0
//   areaLightSelectionPDF(0) = pmf * (1/area) = 1.0 * 2.0 = 2.0
//   lightPDF_sa = 2.0 * 4.0 / 1.0 = 8.0
//   weight = bsdfPDF / (bsdfPDF + 8.0)
//   result = throughput * WHITE_emission * weight
// -----------------------------------------------------------------------

TEST_CASE("pathTrace: BSDF MIS weight = bsdfPDF / (bsdfPDF + lightPDF_sa) analytical") {
    // bsdfPDF=4, lightPDF_sa=8 → weight = 4/12 = 1/3
    FixedBSDF emissiveBSDF(WHITE); emissiveBSDF.emission = WHITE;
    BackgroundColour blackBg(BLACK);
    Triangle lightTri = makeLightTriangle();
    Scene scene; Film film;
    scene.init({lightTri}, {&emissiveBSDF}, &blackBg);
    scene.build();
    scene.width = 64; scene.height = 64; film.init(64, 64);
    PathTracerIntegrator pt(&scene, &film);
    MTRandom sampler;

    Ray ray; ray.init(Vec3(0.25f, 0.0f, 0.25f), Vec3(0.0f, 1.0f, 0.0f));
    float bsdfPDF = 4.0f;
    Colour result = pt.pathTrace(ray, WHITE, 1, bsdfPDF, &sampler, false);

    float expected = bsdfPDF / (bsdfPDF + 8.0f); // = 1/3
    REQUIRE(result.r == Catch::Approx(expected).margin(1e-4f));
    REQUIRE(result.g == Catch::Approx(expected).margin(1e-4f));
    REQUIRE(result.b == Catch::Approx(expected).margin(1e-4f));
}

TEST_CASE("pathTrace: BSDF MIS weight = 0.5 when bsdfPDF equals lightPDF_sa") {
    // bsdfPDF = lightPDF_sa = 8 → weight = 8/16 = 0.5
    FixedBSDF emissiveBSDF(WHITE); emissiveBSDF.emission = WHITE;
    BackgroundColour blackBg(BLACK);
    Triangle lightTri = makeLightTriangle();
    Scene scene; Film film;
    scene.init({lightTri}, {&emissiveBSDF}, &blackBg);
    scene.build();
    scene.width = 64; scene.height = 64; film.init(64, 64);
    PathTracerIntegrator pt(&scene, &film);
    MTRandom sampler;

    Ray ray; ray.init(Vec3(0.25f, 0.0f, 0.25f), Vec3(0.0f, 1.0f, 0.0f));
    Colour result = pt.pathTrace(ray, WHITE, 1, 8.0f, &sampler, false);

    REQUIRE(result.r == Catch::Approx(0.5f).margin(1e-4f));
    REQUIRE(result.g == Catch::Approx(0.5f).margin(1e-4f));
    REQUIRE(result.b == Catch::Approx(0.5f).margin(1e-4f));
}

TEST_CASE("pathTrace: BSDF MIS weight approaches 1 when bsdfPDF dominates") {
    // bsdfPDF=1e5, lightPDF_sa=8 → weight = 1e5 / (1e5+8) ≈ 1 → result ≈ throughput * emission
    FixedBSDF emissiveBSDF(WHITE); emissiveBSDF.emission = WHITE;
    BackgroundColour blackBg(BLACK);
    Triangle lightTri = makeLightTriangle();
    Scene scene; Film film;
    scene.init({lightTri}, {&emissiveBSDF}, &blackBg);
    scene.build();
    scene.width = 64; scene.height = 64; film.init(64, 64);
    PathTracerIntegrator pt(&scene, &film);
    MTRandom sampler;

    Ray ray; ray.init(Vec3(0.25f, 0.0f, 0.25f), Vec3(0.0f, 1.0f, 0.0f));
    Colour result = pt.pathTrace(ray, WHITE, 1, 1e5f, &sampler, false);

    REQUIRE(result.r == Catch::Approx(1.0f).margin(1e-3f));
    REQUIRE(result.g == Catch::Approx(1.0f).margin(1e-3f));
    REQUIRE(result.b == Catch::Approx(1.0f).margin(1e-3f));
}

TEST_CASE("pathTrace: BSDF MIS weight=0 when bsdfPDF=0 — light sampling owns this sample") {
    // bsdfPDF=0, lightPDF_sa=8 → weight = 0/8 = 0 → result = BLACK.
    // The contribution is entirely attributed to light sampling (computeDirectMIS).
    FixedBSDF emissiveBSDF(WHITE); emissiveBSDF.emission = WHITE;
    BackgroundColour blackBg(BLACK);
    Triangle lightTri = makeLightTriangle();
    Scene scene; Film film;
    scene.init({lightTri}, {&emissiveBSDF}, &blackBg);
    scene.build();
    scene.width = 64; scene.height = 64; film.init(64, 64);
    PathTracerIntegrator pt(&scene, &film);
    MTRandom sampler;

    Ray ray; ray.init(Vec3(0.25f, 0.0f, 0.25f), Vec3(0.0f, 1.0f, 0.0f));
    Colour result = pt.pathTrace(ray, WHITE, 1, 0.0f, &sampler, false);

    REQUIRE(result.r == Catch::Approx(0.0f).margin(1e-5f));
    REQUIRE(result.g == Catch::Approx(0.0f).margin(1e-5f));
    REQUIRE(result.b == Catch::Approx(0.0f).margin(1e-5f));
}

TEST_CASE("pathTrace: BSDF MIS result scales proportionally with throughput") {
    // bsdfPDF=8, lightPDF_sa=8 → weight=0.5. Asymmetric throughput exposes missing multiply.
    FixedBSDF emissiveBSDF(WHITE); emissiveBSDF.emission = WHITE;
    BackgroundColour blackBg(BLACK);
    Triangle lightTri = makeLightTriangle();
    Scene scene; Film film;
    scene.init({lightTri}, {&emissiveBSDF}, &blackBg);
    scene.build();
    scene.width = 64; scene.height = 64; film.init(64, 64);
    PathTracerIntegrator pt(&scene, &film);
    MTRandom sampler;

    Ray ray; ray.init(Vec3(0.25f, 0.0f, 0.25f), Vec3(0.0f, 1.0f, 0.0f));
    Colour throughput(0.6f, 0.3f, 0.9f);
    Colour result = pt.pathTrace(ray, throughput, 1, 8.0f, &sampler, false);

    // weight=0.5 → result = throughput * 0.5
    REQUIRE(result.r == Catch::Approx(0.3f).margin(1e-4f));
    REQUIRE(result.g == Catch::Approx(0.15f).margin(1e-4f));
    REQUIRE(result.b == Catch::Approx(0.45f).margin(1e-4f));
}

TEST_CASE("pathTrace: BSDF MIS weight is independent of depth value") {
    // The weight formula only depends on bsdfPDF, distSq, and cosThetaLight — not depth.
    // Same bsdfPDF at depth=1 and depth=2 must produce identical results.
    FixedBSDF emissiveBSDF(WHITE); emissiveBSDF.emission = WHITE;
    BackgroundColour blackBg(BLACK);
    Triangle lightTri = makeLightTriangle();
    Scene scene; Film film;
    scene.init({lightTri}, {&emissiveBSDF}, &blackBg);
    scene.build();
    scene.width = 64; scene.height = 64; film.init(64, 64);
    PathTracerIntegrator pt(&scene, &film);
    MTRandom s1, s2;

    Ray ray; ray.init(Vec3(0.25f, 0.0f, 0.25f), Vec3(0.0f, 1.0f, 0.0f));
    Colour result1 = pt.pathTrace(ray, WHITE, 1, 4.0f, &s1, false);
    Colour result2 = pt.pathTrace(ray, WHITE, 2, 4.0f, &s2, false);

    REQUIRE(result1.r == Catch::Approx(result2.r).margin(1e-5f));
    REQUIRE(result1.g == Catch::Approx(result2.g).margin(1e-5f));
    REQUIRE(result1.b == Catch::Approx(result2.b).margin(1e-5f));
}

// -----------------------------------------------------------------------
// pathTrace: early exits on non-emissive surface hit
//
// Floor at y=0 with MockEnvLight(WHITE, 0.5) as the only light.
// Ray from (0,2,0) down (0,-1,0) → hits floor at t=2, sNormal=(0,1,0), wo=(0,1,0).
//
// directLight with ZeroPDFBSDF (brdfPDF=0 → MIS weight=1):
//   contribution = bsdf(WHITE) * emission(WHITE) * cosTheta(1) / lightPDF(0.5) = 2.0
//
// FixedSampler{0.5f} provides the one value consumed by sampleLightWeighted.
// The miss branch on recursion consumes no sampler values.
// -----------------------------------------------------------------------

TEST_CASE("pathTrace: pdf=0 exits early, returns only directLight * throughput") {
    // ZeroPDFBSDF returns pdf=0 from sample() → guard fires, no indirect contribution.
    // Without the guard: recursive ray adds background light, result ≈ (3,3,3) instead of (2,2,2).
    ZeroPDFBSDF floorBSDF(WHITE);
    MockEnvLight envLight(Vec3(0.0f, 1.0f, 0.0f), WHITE, 0.5f);
    Triangle floorTri = makeFloorTriangle();
    Scene scene; Film film;
    scene.init({floorTri}, {&floorBSDF}, &envLight);
    scene.build();
    scene.width = 64; scene.height = 64; film.init(64, 64);
    PathTracerIntegrator pt(&scene, &film);
    FixedSampler sampler{0.5f};

    Ray ray; ray.init(Vec3(0.0f, 2.0f, 0.0f), Vec3(0.0f, -1.0f, 0.0f));
    Colour result = pt.pathTrace(ray, WHITE, 0, 0.0f, &sampler, false);

    REQUIRE(result.r == Catch::Approx(2.0f).margin(1e-4f));
    REQUIRE(result.g == Catch::Approx(2.0f).margin(1e-4f));
    REQUIRE(result.b == Catch::Approx(2.0f).margin(1e-4f));
}

TEST_CASE("pathTrace: cosTheta < 1e-6 exits early, returns only directLight * throughput") {
    // TangentBSDF returns direction (1,0,0) — perpendicular to sNormal=(0,1,0).
    // cosTheta = |(1,0,0)·(0,1,0)| = 0 < 1e-6 → guard fires, no indirect recursion.
    // TangentBSDF.PDF() = 1.0 → brdfPDF = 1.0 → MIS weight = 0.5/(0.5+1.0) = 1/3.
    // directLight = evaluate(WHITE) * WHITE * cosTheta_shade(1) / lightPDF(0.5) * weight(1/3)
    //             = 1 * 1 * 1 / 0.5 * 1/3 = 2/3.
    // result = directLight * throughput(WHITE) = 2/3.
    TangentBSDF floorBSDF;
    MockEnvLight envLight(Vec3(0.0f, 1.0f, 0.0f), WHITE, 0.5f);
    Triangle floorTri = makeFloorTriangle();
    Scene scene; Film film;
    scene.init({floorTri}, {&floorBSDF}, &envLight);
    scene.build();
    scene.width = 64; scene.height = 64; film.init(64, 64);
    PathTracerIntegrator pt(&scene, &film);
    FixedSampler sampler{0.5f};

    Ray ray; ray.init(Vec3(0.0f, 2.0f, 0.0f), Vec3(0.0f, -1.0f, 0.0f));
    Colour result = pt.pathTrace(ray, WHITE, 0, 0.0f, &sampler, false);

    REQUIRE(result.r == Catch::Approx(2.0f / 3.0f).margin(1e-4f));
    REQUIRE(result.g == Catch::Approx(2.0f / 3.0f).margin(1e-4f));
    REQUIRE(result.b == Catch::Approx(2.0f / 3.0f).margin(1e-4f));
}

// -----------------------------------------------------------------------
// pathTrace: Russian Roulette
//
// Floor + MockEnvLight(WHITE, 0.5). FixedBSDF(WHITE): sample returns
// direction (0,1,0) with pdf=1, indirect=WHITE. No emissive triangles.
//
// directLight with FixedBSDF (brdfPDF=1.0 → MIS weight=1/3):
//   contribution = (bsdf*emission*cosTheta / lightPDF) * weight
//               = (1*1*1 / 0.5) * (1/3) = 2/3
//
// newThroughput = throughput * WHITE = throughput  (for FixedBSDF(WHITE))
// RR at depth=RR_DEPTH: q = lum(throughput), qClamped = min(q, 1)
//
// FixedSampler{sel, rr_eps}: sel consumed by sampleLightWeighted (index 0),
//                             rr_eps consumed by RR check (index 1).
// Recursive ray goes up (0,1,0), misses floor, hits background (WHITE) — no sampler calls.
// -----------------------------------------------------------------------

TEST_CASE("pathTrace: Russian Roulette kills low-throughput path") {
    // throughput=(0.1,0.1,0.1): lum=0.1, qClamped=0.1, epsilon=0.5 > 0.1 → kill.
    // result = directLight * throughput = (2/3) * 0.1 = 1/15 ≈ 0.0667.
    FixedBSDF floorBSDF(WHITE);
    MockEnvLight envLight(Vec3(0.0f, 1.0f, 0.0f), WHITE, 0.5f);
    Triangle floorTri = makeFloorTriangle();
    Scene scene; Film film;
    scene.init({floorTri}, {&floorBSDF}, &envLight);
    scene.build();
    scene.width = 64; scene.height = 64; film.init(64, 64);
    PathTracerIntegrator pt(&scene, &film);
    FixedSampler sampler{0.5f, 0.5f}; // sel=0.5 picks env; rr_eps=0.5 > 0.1 → kill

    Ray ray; ray.init(Vec3(0.0f, 2.0f, 0.0f), Vec3(0.0f, -1.0f, 0.0f));
    Colour throughput(0.1f, 0.1f, 0.1f);
    Colour result = pt.pathTrace(ray, throughput, PathTracerIntegrator::RR_DEPTH, 0.0f, &sampler, false);

    float directLight = 2.0f / 3.0f;
    REQUIRE(result.r == Catch::Approx(directLight * 0.1f).margin(1e-4f));
    REQUIRE(result.g == Catch::Approx(directLight * 0.1f).margin(1e-4f));
    REQUIRE(result.b == Catch::Approx(directLight * 0.1f).margin(1e-4f));
}

TEST_CASE("pathTrace: Russian Roulette boosts surviving path throughput by 1/q") {
    // throughput=(0.5,0.5,0.5): lum=0.5, qClamped=0.5, epsilon=0.1 < 0.5 → survive.
    // newThroughput boosted: (0.5/0.5, 0.5/0.5, 0.5/0.5) = WHITE.
    // Recursive ray (0,1,0) misses floor → returns WHITE * bg(WHITE) = WHITE.
    // result = WHITE(indirect) + (2/3) * (0.5,0.5,0.5)(directLight * throughput)
    //        = (1,1,1) + (1/3,1/3,1/3) = (4/3, 4/3, 4/3) ≈ 1.333
    // Without the boost: newThroughput = (0.5,0.5,0.5), indirect = (0.5,0.5,0.5),
    //   result = (0.5+1/3, ...) = (5/6, ...) ≈ 0.833. Different → verifies the boost.
    FixedBSDF floorBSDF(WHITE);
    MockEnvLight envLight(Vec3(0.0f, 1.0f, 0.0f), WHITE, 0.5f);
    Triangle floorTri = makeFloorTriangle();
    Scene scene; Film film;
    scene.init({floorTri}, {&floorBSDF}, &envLight);
    scene.build();
    scene.width = 64; scene.height = 64; film.init(64, 64);
    PathTracerIntegrator pt(&scene, &film);
    FixedSampler sampler{0.5f, 0.1f}; // sel=0.5 picks env; rr_eps=0.1 < 0.5 → survive

    Ray ray; ray.init(Vec3(0.0f, 2.0f, 0.0f), Vec3(0.0f, -1.0f, 0.0f));
    Colour throughput(0.5f, 0.5f, 0.5f);
    Colour result = pt.pathTrace(ray, throughput, PathTracerIntegrator::RR_DEPTH, 0.0f, &sampler, false);

    float expected = 4.0f / 3.0f;
    REQUIRE(result.r == Catch::Approx(expected).margin(1e-4f));
    REQUIRE(result.g == Catch::Approx(expected).margin(1e-4f));
    REQUIRE(result.b == Catch::Approx(expected).margin(1e-4f));
}

// -----------------------------------------------------------------------
// integrate()
// -----------------------------------------------------------------------

TEST_CASE("integrate: direct emissive hit returns emission (throughput starts at WHITE)") {
    // integrate() calls pathTrace(ray, WHITE, 0, 0.0f, sampler, false).
    // Ray hits WHITE emissive at depth=0 → throughput=WHITE, weight=1 → result=WHITE.
    FixedBSDF emissiveBSDF(WHITE); emissiveBSDF.emission = WHITE;
    BackgroundColour blackBg(BLACK);
    Triangle lightTri = makeLightTriangle();
    Scene scene; Film film;
    scene.init({lightTri}, {&emissiveBSDF}, &blackBg);
    scene.build();
    scene.width = 64; scene.height = 64; film.init(64, 64);
    PathTracerIntegrator pt(&scene, &film);
    MTRandom sampler;

    Ray ray; ray.init(Vec3(0.25f, 0.0f, 0.25f), Vec3(0.0f, 1.0f, 0.0f));
    Colour result = pt.integrate(ray, &sampler);

    REQUIRE(result.r == Catch::Approx(1.0f).margin(1e-5f));
    REQUIRE(result.g == Catch::Approx(1.0f).margin(1e-5f));
    REQUIRE(result.b == Catch::Approx(1.0f).margin(1e-5f));
}

TEST_CASE("integrate: ray miss returns background colour") {
    // Ray going up misses all geometry. integrate() with a coloured background
    // must return that colour (throughput=WHITE so no scaling).
    FixedBSDF dummyBSDF(BLACK);
    BackgroundColour blueBg(Colour(0.0f, 0.0f, 1.0f));
    Triangle dummy = makeDistantTriangle();
    Scene scene; Film film;
    scene.init({dummy}, {&dummyBSDF}, &blueBg);
    scene.build();
    scene.width = 64; scene.height = 64; film.init(64, 64);
    PathTracerIntegrator pt(&scene, &film);
    MTRandom sampler;

    Ray ray; ray.init(Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f));
    Colour result = pt.integrate(ray, &sampler);

    REQUIRE(result.r == Catch::Approx(0.0f).margin(1e-5f));
    REQUIRE(result.g == Catch::Approx(0.0f).margin(1e-5f));
    REQUIRE(result.b == Catch::Approx(1.0f).margin(1e-5f));
}
