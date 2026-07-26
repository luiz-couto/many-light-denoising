#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "integrator.h"
#include "scene.h"
#include "film.h"
#include "geometry.h"
#include "light.h"
#include <cmath>

// -----------------------------------------------------------------------
// Mock BSDF — returns a fixed Colour for evaluate()
// -----------------------------------------------------------------------

class FixedBSDF : public BSDF {
public:
    Colour colour;
    explicit FixedBSDF(Colour c) : colour(c) {}
    Vec3 sample(const ShadingData&, Sampler*, Colour& w, float& pdf) override { w = colour; pdf = 1.f; return Vec3(0,1,0); }
    Colour evaluate(const ShadingData&, const Vec3&) override { return colour; }
    float PDF(const ShadingData&, const Vec3&) override { return 1.f; }
    bool isPureSpecular() override { return false; }
    bool isTwoSided() override { return true; }
    float mask(const ShadingData&) override { return 1.f; }
};

static const Colour WHITE(1.f, 1.f, 1.f);
static const Colour BLACK(0.f, 0.f, 0.f);

// -----------------------------------------------------------------------
// Geometry helpers
// -----------------------------------------------------------------------

// Light triangle at y=2, gNormal=(0,-1,0) facing DOWN toward shading point. Area=0.5.
// Winding: e1=(1,0,0), e2=(0,0,1), e1×e2=(0,-1,0).
static Triangle makeLightTriangle() {
    Vec3 down(0.f, -1.f, 0.f);
    Vertex v0(Vec3(0.f, 2.f, 0.f), down, 0.f, 0.f);
    Vertex v1(Vec3(1.f, 2.f, 0.f), down, 1.f, 0.f);
    Vertex v2(Vec3(0.f, 2.f, 1.f), down, 0.f, 1.f);
    Triangle t; t.init(v0, v1, v2, 0); return t;
}

// Same geometry, reversed winding so gNormal=(0,+1,0) — back-facing to shading point at y=0.
// Winding: e1=(0,0,1), e2=(1,0,0), e1×e2=(0,+1,0).
static Triangle makeBackFacingLightTriangle() {
    Vec3 up(0.f, 1.f, 0.f);
    Vertex v0(Vec3(0.f, 2.f, 0.f), up, 0.f, 0.f);
    Vertex v1(Vec3(0.f, 2.f, 1.f), up, 0.f, 1.f);
    Vertex v2(Vec3(1.f, 2.f, 0.f), up, 1.f, 0.f);
    Triangle t; t.init(v0, v1, v2, 0); return t;
}

// Opaque blocker at y=1: covers all shadow rays from (0.25,0,0.25) to the light at y=2.
static Triangle makeBlockerTriangle() {
    Vec3 up(0.f, 1.f, 0.f);
    Vertex v0(Vec3(-1.f, 1.f, -1.f), up, 0.f, 0.f);
    Vertex v1(Vec3( 2.f, 1.f, -1.f), up, 1.f, 0.f);
    Vertex v2(Vec3(-1.f, 1.f,  2.f), up, 0.f, 1.f);
    Triangle t; t.init(v0, v1, v2, 0); return t;
}

// Dummy triangle far below — ensures BVH is non-empty without blocking upward shadow rays.
static Triangle makeDistantTriangle() {
    Vec3 up(0.f, 1.f, 0.f);
    Vertex v0(Vec3(-100.f, -100.f, -100.f), up, 0.f, 0.f);
    Vertex v1(Vec3( 100.f, -100.f, -100.f), up, 1.f, 0.f);
    Vertex v2(Vec3(-100.f, -100.f,  100.f), up, 0.f, 1.f);
    Triangle t; t.init(v0, v1, v2, 0); return t;
}

// Shading point at (0.25, 0, 0.25), sNormal=(0,1,0) facing up toward light at y=2.
static ShadingData makeSD(BSDF* bsdf) {
    ShadingData sd(Vec3(0.25f, 0.f, 0.25f), Vec3(0.f, 1.f, 0.f));
    sd.bsdf = bsdf;
    return sd;
}

// Same position, sNormal=(0,-1,0) — back-facing relative to light above.
static ShadingData makeDownFacingSD(BSDF* bsdf) {
    ShadingData sd(Vec3(0.25f, 0.f, 0.25f), Vec3(0.f, -1.f, 0.f));
    sd.bsdf = bsdf;
    return sd;
}

// -----------------------------------------------------------------------
// lightSamplingMISAreaLight tests
// -----------------------------------------------------------------------

TEST_CASE("lightSamplingMISAreaLight: wi is normalized") {
    FixedBSDF bsdf(WHITE); BackgroundColour bg(BLACK);
    Triangle dummy = makeDistantTriangle();
    Triangle lightTri = makeLightTriangle();
    AreaLight light; light.triangle = &lightTri; light.emission = WHITE;

    Scene scene; Film film;
    scene.init({dummy}, {&bsdf}, &bg);
    scene.build();
    scene.width = 64; scene.height = 64;
    film.init(64, 64);
    Integrator integrator(&scene, &film);
    ShadingData sd = makeSD(&bsdf);
    MTRandom sampler;

    for (int i = 0; i < 10; i++) {
        auto r = integrator.lightSamplingMISAreaLight(sd, &sampler, &light, 1.f);
        float len = sqrtf(r.wi.x*r.wi.x + r.wi.y*r.wi.y + r.wi.z*r.wi.z);
        REQUIRE(len == Catch::Approx(1.f).margin(1e-4f));
    }
}

TEST_CASE("lightSamplingMISAreaLight: wi points toward light above shading point") {
    FixedBSDF bsdf(WHITE); BackgroundColour bg(BLACK);
    Triangle dummy = makeDistantTriangle();
    Triangle lightTri = makeLightTriangle();
    AreaLight light; light.triangle = &lightTri; light.emission = WHITE;

    Scene scene; Film film;
    scene.init({dummy}, {&bsdf}, &bg);
    scene.build();
    scene.width = 64; scene.height = 64;
    film.init(64, 64);
    Integrator integrator(&scene, &film);
    ShadingData sd = makeSD(&bsdf);
    MTRandom sampler;

    for (int i = 0; i < 10; i++) {
        auto r = integrator.lightSamplingMISAreaLight(sd, &sampler, &light, 1.f);
        REQUIRE(r.wi.y > 0.f);
    }
}

TEST_CASE("lightSamplingMISAreaLight: pdf equals pmf times area pdf (1/area)") {
    // Triangle area = 0.5 → pdf_area = 2.0. With pmf=1: result.pdf must be 2.0.
    FixedBSDF bsdf(WHITE); BackgroundColour bg(BLACK);
    Triangle dummy = makeDistantTriangle();
    Triangle lightTri = makeLightTriangle();
    AreaLight light; light.triangle = &lightTri; light.emission = WHITE;
    float expectedPDF = 1.f / lightTri.area;   // 2.0

    Scene scene; Film film;
    scene.init({dummy}, {&bsdf}, &bg);
    scene.build();
    scene.width = 64; scene.height = 64;
    film.init(64, 64);
    Integrator integrator(&scene, &film);
    ShadingData sd = makeSD(&bsdf);
    MTRandom sampler;

    for (int i = 0; i < 5; i++) {
        auto r = integrator.lightSamplingMISAreaLight(sd, &sampler, &light, 1.f);
        REQUIRE(r.pdf == Catch::Approx(expectedPDF).margin(1e-5f));
    }
}

TEST_CASE("lightSamplingMISAreaLight: pdf scales proportionally with pmf") {
    FixedBSDF bsdf(WHITE); BackgroundColour bg(BLACK);
    Triangle dummy = makeDistantTriangle();
    Triangle lightTri = makeLightTriangle();
    AreaLight light; light.triangle = &lightTri; light.emission = WHITE;

    Scene scene; Film film;
    scene.init({dummy}, {&bsdf}, &bg);
    scene.build();
    scene.width = 64; scene.height = 64;
    film.init(64, 64);
    Integrator integrator(&scene, &film);
    ShadingData sd = makeSD(&bsdf);
    MTRandom s1(1), s2(1);  // identical seeds → identical sampled point

    auto r1 = integrator.lightSamplingMISAreaLight(sd, &s1, &light, 1.0f);
    auto r2 = integrator.lightSamplingMISAreaLight(sd, &s2, &light, 0.5f);

    REQUIRE(r2.pdf == Catch::Approx(r1.pdf * 0.5f).margin(1e-5f));
}

TEST_CASE("lightSamplingMISAreaLight: gTerm equals cosTheta * cosThetaLine / distSqr") {
    FixedBSDF bsdf(WHITE); BackgroundColour bg(BLACK);
    Triangle dummy = makeDistantTriangle();
    Triangle lightTri = makeLightTriangle();
    AreaLight light; light.triangle = &lightTri; light.emission = WHITE;

    Scene scene; Film film;
    scene.init({dummy}, {&bsdf}, &bg);
    scene.build();
    scene.width = 64; scene.height = 64;
    film.init(64, 64);
    Integrator integrator(&scene, &film);
    ShadingData sd = makeSD(&bsdf);
    MTRandom sampler;

    for (int i = 0; i < 10; i++) {
        auto r = integrator.lightSamplingMISAreaLight(sd, &sampler, &light, 1.f);
        float cosTheta = sd.sNormal.dot(r.wi);
        if (cosTheta < 0.f) cosTheta = 0.f;
        float expected = (cosTheta * r.cosThetaLine) / r.distSqr;
        REQUIRE(r.gTerm == Catch::Approx(expected).margin(1e-5f));
    }
}

TEST_CASE("lightSamplingMISAreaLight: finalColor equals bsdf * emission * gTerm for white inputs") {
    // bsdf->evaluate returns WHITE (1,1,1), emission = WHITE, visible = true.
    // finalColor = WHITE * WHITE * gTerm = (gTerm, gTerm, gTerm).
    FixedBSDF bsdf(WHITE); BackgroundColour bg(BLACK);
    Triangle dummy = makeDistantTriangle();
    Triangle lightTri = makeLightTriangle();
    AreaLight light; light.triangle = &lightTri; light.emission = WHITE;

    Scene scene; Film film;
    scene.init({dummy}, {&bsdf}, &bg);
    scene.build();
    scene.width = 64; scene.height = 64;
    film.init(64, 64);
    Integrator integrator(&scene, &film);
    ShadingData sd = makeSD(&bsdf);
    MTRandom sampler;

    for (int i = 0; i < 5; i++) {
        auto r = integrator.lightSamplingMISAreaLight(sd, &sampler, &light, 1.f);
        REQUIRE(r.finalColor.r == Catch::Approx(r.gTerm).margin(1e-5f));
        REQUIRE(r.finalColor.g == Catch::Approx(r.gTerm).margin(1e-5f));
        REQUIRE(r.finalColor.b == Catch::Approx(r.gTerm).margin(1e-5f));
    }
}

TEST_CASE("lightSamplingMISAreaLight: cosThetaLine is in [0, 1] for valid configuration") {
    FixedBSDF bsdf(WHITE); BackgroundColour bg(BLACK);
    Triangle dummy = makeDistantTriangle();
    Triangle lightTri = makeLightTriangle();
    AreaLight light; light.triangle = &lightTri; light.emission = WHITE;

    Scene scene; Film film;
    scene.init({dummy}, {&bsdf}, &bg);
    scene.build();
    scene.width = 64; scene.height = 64;
    film.init(64, 64);
    Integrator integrator(&scene, &film);
    ShadingData sd = makeSD(&bsdf);
    MTRandom sampler;

    for (int i = 0; i < 10; i++) {
        auto r = integrator.lightSamplingMISAreaLight(sd, &sampler, &light, 1.f);
        REQUIRE(r.cosThetaLine >= 0.f);
        REQUIRE(r.cosThetaLine <= 1.f);
    }
}

TEST_CASE("lightSamplingMISAreaLight: finalColor is black when light is back-facing") {
    // Light gNormal = (0,+1,0): pointing away from shading point below.
    // wi is upward, so cosThetaLine = -wi.dot(0,1,0) = -wi.y < 0 → clamped to 0.
    FixedBSDF bsdf(WHITE); BackgroundColour bg(BLACK);
    Triangle dummy = makeDistantTriangle();
    Triangle backFacingTri = makeBackFacingLightTriangle();
    AreaLight light; light.triangle = &backFacingTri; light.emission = WHITE;

    Scene scene; Film film;
    scene.init({dummy}, {&bsdf}, &bg);
    scene.build();
    scene.width = 64; scene.height = 64;
    film.init(64, 64);
    Integrator integrator(&scene, &film);
    ShadingData sd = makeSD(&bsdf);
    MTRandom sampler;

    for (int i = 0; i < 5; i++) {
        auto r = integrator.lightSamplingMISAreaLight(sd, &sampler, &light, 1.f);
        REQUIRE(r.cosThetaLine == Catch::Approx(0.f).margin(1e-5f));
        REQUIRE(r.finalColor.r == Catch::Approx(0.f).margin(1e-5f));
        REQUIRE(r.finalColor.g == Catch::Approx(0.f).margin(1e-5f));
        REQUIRE(r.finalColor.b == Catch::Approx(0.f).margin(1e-5f));
    }
}

TEST_CASE("lightSamplingMISAreaLight: finalColor is black when surface faces away from light") {
    // sNormal = (0,-1,0): wi.y > 0 but cosTheta = (0,-1,0)·wi = -wi.y < 0 → clamped to 0.
    FixedBSDF bsdf(WHITE); BackgroundColour bg(BLACK);
    Triangle dummy = makeDistantTriangle();
    Triangle lightTri = makeLightTriangle();
    AreaLight light; light.triangle = &lightTri; light.emission = WHITE;

    Scene scene; Film film;
    scene.init({dummy}, {&bsdf}, &bg);
    scene.build();
    scene.width = 64; scene.height = 64;
    film.init(64, 64);
    Integrator integrator(&scene, &film);
    ShadingData sd = makeDownFacingSD(&bsdf);
    MTRandom sampler;

    for (int i = 0; i < 5; i++) {
        auto r = integrator.lightSamplingMISAreaLight(sd, &sampler, &light, 1.f);
        REQUIRE(r.gTerm == Catch::Approx(0.f).margin(1e-5f));
        REQUIRE(r.finalColor.r == Catch::Approx(0.f).margin(1e-5f));
        REQUIRE(r.finalColor.g == Catch::Approx(0.f).margin(1e-5f));
        REQUIRE(r.finalColor.b == Catch::Approx(0.f).margin(1e-5f));
    }
}

TEST_CASE("lightSamplingMISAreaLight: finalColor is black when light is occluded") {
    // Blocker at y=1 sits between shading point (y=0) and light (y=2).
    FixedBSDF bsdf(WHITE); BackgroundColour bg(BLACK);
    Triangle blocker = makeBlockerTriangle();
    Triangle lightTri = makeLightTriangle();
    AreaLight light; light.triangle = &lightTri; light.emission = WHITE;

    Scene scene; Film film;
    scene.init({blocker}, {&bsdf}, &bg);
    scene.build();
    scene.width = 64; scene.height = 64;
    film.init(64, 64);
    Integrator integrator(&scene, &film);
    ShadingData sd = makeSD(&bsdf);
    MTRandom sampler;

    for (int i = 0; i < 10; i++) {
        auto r = integrator.lightSamplingMISAreaLight(sd, &sampler, &light, 1.f);
        REQUIRE(r.finalColor.r == Catch::Approx(0.f).margin(1e-5f));
        REQUIRE(r.finalColor.g == Catch::Approx(0.f).margin(1e-5f));
        REQUIRE(r.finalColor.b == Catch::Approx(0.f).margin(1e-5f));
    }
}

TEST_CASE("lightSamplingMISAreaLight: finalColor is non-zero for valid unoccluded setup") {
    FixedBSDF bsdf(WHITE); BackgroundColour bg(BLACK);
    Triangle dummy = makeDistantTriangle();
    Triangle lightTri = makeLightTriangle();
    AreaLight light; light.triangle = &lightTri; light.emission = WHITE;

    Scene scene; Film film;
    scene.init({dummy}, {&bsdf}, &bg);
    scene.build();
    scene.width = 64; scene.height = 64;
    film.init(64, 64);
    Integrator integrator(&scene, &film);
    ShadingData sd = makeSD(&bsdf);
    MTRandom sampler;

    auto r = integrator.lightSamplingMISAreaLight(sd, &sampler, &light, 1.f);
    REQUIRE(r.finalColor.r > 0.f);
}

TEST_CASE("lightSamplingMISAreaLight: doubling emission doubles finalColor") {
    // Same sampler seed → same random point on triangle. emission=2 must give 2× finalColor.
    FixedBSDF bsdf(WHITE); BackgroundColour bg(BLACK);
    Triangle dummy = makeDistantTriangle();
    Triangle lightTri = makeLightTriangle();
    AreaLight light1; light1.triangle = &lightTri; light1.emission = WHITE;
    AreaLight light2; light2.triangle = &lightTri; light2.emission = Colour(2.f, 2.f, 2.f);

    Scene scene; Film film;
    scene.init({dummy}, {&bsdf}, &bg);
    scene.build();
    scene.width = 64; scene.height = 64;
    film.init(64, 64);
    Integrator integrator(&scene, &film);
    ShadingData sd = makeSD(&bsdf);
    MTRandom s1(1), s2(1);  // identical seeds → identical sampled point

    auto r1 = integrator.lightSamplingMISAreaLight(sd, &s1, &light1, 1.f);
    auto r2 = integrator.lightSamplingMISAreaLight(sd, &s2, &light2, 1.f);

    REQUIRE(r2.finalColor.r == Catch::Approx(r1.finalColor.r * 2.f).margin(1e-5f));
    REQUIRE(r2.finalColor.g == Catch::Approx(r1.finalColor.g * 2.f).margin(1e-5f));
    REQUIRE(r2.finalColor.b == Catch::Approx(r1.finalColor.b * 2.f).margin(1e-5f));
}

// -----------------------------------------------------------------------
// Additional mocks for lightSamplingMISEnvMap and lightSamplingMIS
// -----------------------------------------------------------------------

// Env light that always samples a fixed direction with known emission and pdf.
// Ignores the sampler so tests are fully deterministic without a FixedSampler.
class MockEnvLight : public Light {
public:
    Vec3   dir;
    Colour emission;
    float  pdfVal;

    MockEnvLight(Vec3 d, Colour e, float p) : dir(d.normalize()), emission(e), pdfVal(p) {}

    Vec3   sample(const ShadingData&, Sampler*, Colour& e, float& pdf) override
    { e = emission; pdf = pdfVal; return dir; }
    Colour evaluate(const Vec3&) override { return emission; }
    float  PDF(const ShadingData&, const Vec3&) override { return pdfVal; }
    bool   isArea() override { return false; }
    Vec3   normal(const ShadingData&, const Vec3&) override { return Vec3(0,1,0); }
    float  totalIntegratedPower() override { return emission.lum() * 4.0f * (float)M_PI; }
    Vec3   samplePositionFromLight(Sampler*, float& pdf) override { pdf = 1.f; return dir * 1000.f; }
    Vec3   sampleDirectionFromLight(Sampler*, float& pdf) override { pdf = pdfVal; return dir; }
};

// BSDF with evaluate = colour but PDF = 0.
// With a zero BSDF PDF the MIS weight collapses to 1, making the contribution
// exactly finalColor / lightPDF — easy to verify analytically.
class ZeroPDFBSDF : public BSDF {
public:
    Colour colour;
    explicit ZeroPDFBSDF(Colour c) : colour(c) {}
    Vec3   sample(const ShadingData&, Sampler*, Colour& w, float& pdf) override
    { w = colour; pdf = 0.f; return Vec3(0,1,0); }
    Colour evaluate(const ShadingData&, const Vec3&) override { return colour; }
    float  PDF(const ShadingData&, const Vec3&) override { return 0.f; }
    bool   isPureSpecular() override { return false; }
    bool   isTwoSided() override { return true; }
    float  mask(const ShadingData&) override { return 1.f; }
};

// Sampler that returns a fixed, wrapping sequence of values.
// Used to make Triangle::sample() and sampleLightWeighted() deterministic.
class FixedSampler : public Sampler {
    std::vector<float> values;
    size_t idx = 0;
public:
    explicit FixedSampler(std::initializer_list<float> v) : values(v) {}
    float next() override { float v = values[idx % values.size()]; ++idx; return v; }
};

// Second light triangle shifted +2 in X, same area=0.5, materialIndex=0.
// Used together with makeLightTriangle() to create a two-light scene with pmf=0.5 each.
static Triangle makeSecondLightTriangle() {
    Vec3 down(0.f, -1.f, 0.f);
    Vertex v0(Vec3(2.f, 2.f, 0.f), down, 0.f, 0.f);
    Vertex v1(Vec3(3.f, 2.f, 0.f), down, 1.f, 0.f);
    Vertex v2(Vec3(2.f, 2.f, 1.f), down, 0.f, 1.f);
    Triangle t; t.init(v0, v1, v2, 0); return t;
}

// -----------------------------------------------------------------------
// lightSamplingMISEnvMap tests
//
// MockEnvLight always returns wi=(0,1,0), emission=WHITE, pdf=0.5.
// Shading point at (0.25,0,0.25), sNormal=(0,1,0) → cosTheta=1 → gTerm=1.
// -----------------------------------------------------------------------

TEST_CASE("lightSamplingMISEnvMap: wi equals the direction sampled from the env light") {
    FixedBSDF bsdf(WHITE); BackgroundColour blackBg(BLACK);
    Triangle dummy = makeDistantTriangle();
    Scene scene; Film film;
    scene.init({dummy}, {&bsdf}, &blackBg);
    scene.build();
    scene.width = 64; scene.height = 64; film.init(64, 64);
    Integrator integrator(&scene, &film);
    MockEnvLight envLight(Vec3(0,1,0), WHITE, 0.5f);
    ShadingData sd = makeSD(&bsdf);
    MTRandom sampler;

    auto r = integrator.lightSamplingMISEnvMap(sd, &sampler, &envLight, 1.0f);
    REQUIRE(r.wi.x == Catch::Approx(0.f).margin(1e-5f));
    REQUIRE(r.wi.y == Catch::Approx(1.f).margin(1e-5f));
    REQUIRE(r.wi.z == Catch::Approx(0.f).margin(1e-5f));
}

TEST_CASE("lightSamplingMISEnvMap: pdf equals pmf times env light pdf") {
    FixedBSDF bsdf(WHITE); BackgroundColour blackBg(BLACK);
    Triangle dummy = makeDistantTriangle();
    Scene scene; Film film;
    scene.init({dummy}, {&bsdf}, &blackBg);
    scene.build();
    scene.width = 64; scene.height = 64; film.init(64, 64);
    Integrator integrator(&scene, &film);
    MockEnvLight envLight(Vec3(0,1,0), WHITE, 0.5f);
    ShadingData sd = makeSD(&bsdf);
    MTRandom sampler;

    // pmf=0.4, envPDF=0.5 → fullPdf must be 0.2
    auto r = integrator.lightSamplingMISEnvMap(sd, &sampler, &envLight, 0.4f);
    REQUIRE(r.pdf == Catch::Approx(0.4f * 0.5f).margin(1e-5f));
}

TEST_CASE("lightSamplingMISEnvMap: cosThetaLine=1 and distSqr=1 are measure-alignment sentinels") {
    // In lightSamplingMIS the BSDF PDF is converted via brdfPDF * cosThetaLine/distSqr.
    // For env maps both must be 1 so the conversion is a no-op and both PDFs stay in solid angle.
    FixedBSDF bsdf(WHITE); BackgroundColour blackBg(BLACK);
    Triangle dummy = makeDistantTriangle();
    Scene scene; Film film;
    scene.init({dummy}, {&bsdf}, &blackBg);
    scene.build();
    scene.width = 64; scene.height = 64; film.init(64, 64);
    Integrator integrator(&scene, &film);
    MockEnvLight envLight(Vec3(0,1,0), WHITE, 0.5f);
    ShadingData sd = makeSD(&bsdf);
    MTRandom sampler;

    auto r = integrator.lightSamplingMISEnvMap(sd, &sampler, &envLight, 1.0f);
    REQUIRE(r.cosThetaLine == Catch::Approx(1.0f).margin(1e-5f));
    REQUIRE(r.distSqr      == Catch::Approx(1.0f).margin(1e-5f));
}

TEST_CASE("lightSamplingMISEnvMap: gTerm equals cosTheta for normal-incidence direction") {
    // wi = sNormal = (0,1,0) → cosTheta = 1.0 → gTerm = 1.0
    FixedBSDF bsdf(WHITE); BackgroundColour blackBg(BLACK);
    Triangle dummy = makeDistantTriangle();
    Scene scene; Film film;
    scene.init({dummy}, {&bsdf}, &blackBg);
    scene.build();
    scene.width = 64; scene.height = 64; film.init(64, 64);
    Integrator integrator(&scene, &film);
    MockEnvLight envLight(Vec3(0,1,0), WHITE, 0.5f);
    ShadingData sd = makeSD(&bsdf);
    MTRandom sampler;

    auto r = integrator.lightSamplingMISEnvMap(sd, &sampler, &envLight, 1.0f);
    REQUIRE(r.gTerm == Catch::Approx(1.0f).margin(1e-5f));
}

TEST_CASE("lightSamplingMISEnvMap: gTerm equals cosTheta at 45 degrees") {
    // wi = normalize(1,1,0), sNormal = (0,1,0) → cosTheta = 1/√2 ≈ 0.7071
    FixedBSDF bsdf(WHITE); BackgroundColour blackBg(BLACK);
    Triangle dummy = makeDistantTriangle();
    Scene scene; Film film;
    scene.init({dummy}, {&bsdf}, &blackBg);
    scene.build();
    scene.width = 64; scene.height = 64; film.init(64, 64);
    Integrator integrator(&scene, &film);
    MockEnvLight envLight(Vec3(1,1,0), WHITE, 0.5f);  // constructor normalizes
    ShadingData sd = makeSD(&bsdf);
    MTRandom sampler;

    auto r = integrator.lightSamplingMISEnvMap(sd, &sampler, &envLight, 1.0f);
    REQUIRE(r.gTerm == Catch::Approx(1.0f / sqrtf(2.0f)).margin(1e-4f));
}

TEST_CASE("lightSamplingMISEnvMap: finalColor equals bsdf * emission * cosTheta when visible") {
    // WHITE bsdf, WHITE emission, cosTheta=1, visible → finalColor=(1,1,1)
    FixedBSDF bsdf(WHITE); BackgroundColour blackBg(BLACK);
    Triangle dummy = makeDistantTriangle();
    Scene scene; Film film;
    scene.init({dummy}, {&bsdf}, &blackBg);
    scene.build();
    scene.width = 64; scene.height = 64; film.init(64, 64);
    Integrator integrator(&scene, &film);
    MockEnvLight envLight(Vec3(0,1,0), WHITE, 0.5f);
    ShadingData sd = makeSD(&bsdf);
    MTRandom sampler;

    auto r = integrator.lightSamplingMISEnvMap(sd, &sampler, &envLight, 1.0f);
    REQUIRE(r.finalColor.r == Catch::Approx(1.0f).margin(1e-5f));
    REQUIRE(r.finalColor.g == Catch::Approx(1.0f).margin(1e-5f));
    REQUIRE(r.finalColor.b == Catch::Approx(1.0f).margin(1e-5f));
}

TEST_CASE("lightSamplingMISEnvMap: gTerm is pre-visibility — non-zero even when path is occluded") {
    // gTerm = cosTheta, computed before the visibility test. finalColor is what gets zeroed.
    FixedBSDF bsdf(WHITE); BackgroundColour blackBg(BLACK);
    Triangle blocker = makeBlockerTriangle();
    Scene scene; Film film;
    scene.init({blocker}, {&bsdf}, &blackBg);
    scene.build();
    scene.width = 64; scene.height = 64; film.init(64, 64);
    Integrator integrator(&scene, &film);
    MockEnvLight envLight(Vec3(0,1,0), WHITE, 0.5f);
    ShadingData sd = makeSD(&bsdf);
    MTRandom sampler;

    auto r = integrator.lightSamplingMISEnvMap(sd, &sampler, &envLight, 1.0f);
    REQUIRE(r.gTerm        == Catch::Approx(1.0f).margin(1e-5f));  // cosTheta=1, unaffected
    REQUIRE(r.finalColor.r == Catch::Approx(0.0f).margin(1e-5f));  // blocked
}

TEST_CASE("lightSamplingMISEnvMap: finalColor is black when surface back-faces the sampled direction") {
    // sNormal=(0,-1,0), wi=(0,1,0) → cosTheta=-1 → clamped to 0 → gTerm=0 → finalColor=0
    FixedBSDF bsdf(WHITE); BackgroundColour blackBg(BLACK);
    Triangle dummy = makeDistantTriangle();
    Scene scene; Film film;
    scene.init({dummy}, {&bsdf}, &blackBg);
    scene.build();
    scene.width = 64; scene.height = 64; film.init(64, 64);
    Integrator integrator(&scene, &film);
    MockEnvLight envLight(Vec3(0,1,0), WHITE, 0.5f);
    ShadingData sd = makeDownFacingSD(&bsdf);
    MTRandom sampler;

    auto r = integrator.lightSamplingMISEnvMap(sd, &sampler, &envLight, 1.0f);
    REQUIRE(r.gTerm        == Catch::Approx(0.0f).margin(1e-5f));
    REQUIRE(r.finalColor.r == Catch::Approx(0.0f).margin(1e-5f));
    REQUIRE(r.finalColor.g == Catch::Approx(0.0f).margin(1e-5f));
    REQUIRE(r.finalColor.b == Catch::Approx(0.0f).margin(1e-5f));
}

TEST_CASE("lightSamplingMISEnvMap: finalColor is black when occluded") {
    // Blocker at y=1 sits between shading point (y=0) and the far environment sample point.
    FixedBSDF bsdf(WHITE); BackgroundColour blackBg(BLACK);
    Triangle blocker = makeBlockerTriangle();
    Scene scene; Film film;
    scene.init({blocker}, {&bsdf}, &blackBg);
    scene.build();
    scene.width = 64; scene.height = 64; film.init(64, 64);
    Integrator integrator(&scene, &film);
    MockEnvLight envLight(Vec3(0,1,0), WHITE, 0.5f);
    ShadingData sd = makeSD(&bsdf);
    MTRandom sampler;

    auto r = integrator.lightSamplingMISEnvMap(sd, &sampler, &envLight, 1.0f);
    REQUIRE(r.finalColor.r == Catch::Approx(0.0f).margin(1e-5f));
    REQUIRE(r.finalColor.g == Catch::Approx(0.0f).margin(1e-5f));
    REQUIRE(r.finalColor.b == Catch::Approx(0.0f).margin(1e-5f));
}

TEST_CASE("lightSamplingMISEnvMap: doubling emission doubles finalColor") {
    FixedBSDF bsdf(WHITE); BackgroundColour blackBg(BLACK);
    Triangle dummy = makeDistantTriangle();
    Scene scene; Film film;
    scene.init({dummy}, {&bsdf}, &blackBg);
    scene.build();
    scene.width = 64; scene.height = 64; film.init(64, 64);
    Integrator integrator(&scene, &film);
    ShadingData sd = makeSD(&bsdf);
    MTRandom s1, s2;

    MockEnvLight env1(Vec3(0,1,0), WHITE,                  0.5f);
    MockEnvLight env2(Vec3(0,1,0), Colour(2.f, 2.f, 2.f), 0.5f);

    auto r1 = integrator.lightSamplingMISEnvMap(sd, &s1, &env1, 1.0f);
    auto r2 = integrator.lightSamplingMISEnvMap(sd, &s2, &env2, 1.0f);

    REQUIRE(r2.finalColor.r == Catch::Approx(r1.finalColor.r * 2.f).margin(1e-5f));
    REQUIRE(r2.finalColor.g == Catch::Approx(r1.finalColor.g * 2.f).margin(1e-5f));
    REQUIRE(r2.finalColor.b == Catch::Approx(r1.finalColor.b * 2.f).margin(1e-5f));
}

// -----------------------------------------------------------------------
// lightSamplingMIS tests
//
// MIS balance heuristic: weight = lightPDF / (lightPDF + brdfPDF_converted)
// Area lights: brdfPDF_area = brdfPDF_sa * cosThetaLight / distSqr
// Env maps:    cosThetaLine=1, distSqr=1 (sentinels) → brdfPDF unchanged (solid angle)
//
// Analytical base case — FixedSampler{0.5, 0.25, 0.5} with ONE area light:
//   sampleLightWeighted reads 0.5 → selects the only light, pmf=1.0
//   Triangle::sample reads 0.25 (r1) then 0.5 (r2):
//     sqrtR1=0.5, α=0.5, β=0.25, γ=0.25
//     sampledPoint = (0,2,0)·0.5 + (1,2,0)·0.25 + (0,2,1)·0.25 = (0.25,2,0.25)
//   From shading point (0.25,0,0.25):
//     wi=(0,1,0), distSqr=4, cosThetaLight=1, cosTheta=1, gTerm=0.25, lightPDF=2.0
// -----------------------------------------------------------------------

static constexpr float FS_SEL = 0.5f;   // consumed by sampleLightWeighted
static constexpr float FS_R1  = 0.25f;  // Triangle r1: sqrtR1=0.5
static constexpr float FS_R2  = 0.5f;   // Triangle r2: beta=0.25

static constexpr float BASE_LIGHT_PDF   = 2.0f;   // pmf=1 * pdf_area=1/0.5=2
static constexpr float BASE_DIST_SQR    = 4.0f;   // |(0,2,0)|² = 4
static constexpr float BASE_COS_THETA_L = 1.0f;   // -wi·lightNormal = 1
static constexpr float BASE_G_TERM      = 0.25f;  // cosTheta·cosThetaLight/distSqr = 1·1/4

TEST_CASE("lightSamplingMIS area light: result is positive for valid unoccluded configuration") {
    FixedBSDF emissiveBSDF(WHITE); emissiveBSDF.emission = WHITE;
    Triangle lightTri = makeLightTriangle();
    BackgroundColour blackBg(BLACK);
    Scene scene; Film film;
    scene.init({lightTri}, {&emissiveBSDF}, &blackBg);
    scene.build();
    scene.width = 64; scene.height = 64; film.init(64, 64);
    Integrator integrator(&scene, &film);
    ShadingData sd = makeSD(&emissiveBSDF);
    MTRandom sampler;

    Colour result = BLACK;
    integrator.lightSamplingMIS(sd, &sampler, result);
    REQUIRE(result.r > 0.f);
}

TEST_CASE("lightSamplingMIS area light: result is black when surface faces away from light") {
    FixedBSDF emissiveBSDF(WHITE); emissiveBSDF.emission = WHITE;
    Triangle lightTri = makeLightTriangle();
    BackgroundColour blackBg(BLACK);
    Scene scene; Film film;
    scene.init({lightTri}, {&emissiveBSDF}, &blackBg);
    scene.build();
    scene.width = 64; scene.height = 64; film.init(64, 64);
    Integrator integrator(&scene, &film);
    ShadingData sd = makeDownFacingSD(&emissiveBSDF);
    MTRandom sampler;

    Colour result = BLACK;
    integrator.lightSamplingMIS(sd, &sampler, result);
    REQUIRE(result.r == Catch::Approx(0.f).margin(1e-5f));
    REQUIRE(result.g == Catch::Approx(0.f).margin(1e-5f));
    REQUIRE(result.b == Catch::Approx(0.f).margin(1e-5f));
}

TEST_CASE("lightSamplingMIS area light: result is black when light is occluded") {
    FixedBSDF emissiveBSDF(WHITE); emissiveBSDF.emission = WHITE;
    FixedBSDF blockerBSDF(BLACK);
    Triangle lightTri = makeLightTriangle();
    Triangle blocker  = makeBlockerTriangle();
    blocker.materialIndex = 1;
    BackgroundColour blackBg(BLACK);
    Scene scene; Film film;
    scene.init({lightTri, blocker}, {&emissiveBSDF, &blockerBSDF}, &blackBg);
    scene.build();
    scene.width = 64; scene.height = 64; film.init(64, 64);
    Integrator integrator(&scene, &film);
    ShadingData sd = makeSD(&emissiveBSDF);
    MTRandom sampler;

    Colour result = BLACK;
    integrator.lightSamplingMIS(sd, &sampler, result);
    REQUIRE(result.r == Catch::Approx(0.f).margin(1e-5f));
    REQUIRE(result.g == Catch::Approx(0.f).margin(1e-5f));
    REQUIRE(result.b == Catch::Approx(0.f).margin(1e-5f));
}

TEST_CASE("lightSamplingMIS area light: analytical — weight=1 when brdfPDF=0, contribution=gTerm/lightPDF") {
    // ZeroPDFBSDF: brdfPDF_area = 0 → weight = lightPDF/(lightPDF+0) = 1
    // contribution = finalColor/lightPDF = bsdf·emission·gTerm / lightPDF
    //             = WHITE·WHITE·0.25 / 2.0 = 0.125
    FixedBSDF emissiveBSDF(WHITE); emissiveBSDF.emission = WHITE;
    ZeroPDFBSDF shadingBSDF(WHITE);
    Triangle lightTri = makeLightTriangle();
    BackgroundColour blackBg(BLACK);
    Scene scene; Film film;
    scene.init({lightTri}, {&emissiveBSDF}, &blackBg);
    scene.build();
    scene.width = 64; scene.height = 64; film.init(64, 64);
    Integrator integrator(&scene, &film);
    ShadingData sd = makeSD(&shadingBSDF);
    FixedSampler sampler{FS_SEL, FS_R1, FS_R2};

    Colour result = BLACK;
    integrator.lightSamplingMIS(sd, &sampler, result);

    // weight=1 → contribution = gTerm / lightPDF = 0.25 / 2.0 = 0.125
    REQUIRE(result.r == Catch::Approx(0.125f).margin(1e-4f));
    REQUIRE(result.g == Catch::Approx(0.125f).margin(1e-4f));
    REQUIRE(result.b == Catch::Approx(0.125f).margin(1e-4f));
}

TEST_CASE("lightSamplingMIS area light: analytical — brdfPDF is correctly converted to area measure") {
    // FixedBSDF PDF=1.0 (solid angle). Convert: brdfPDF_area = 1.0 * cosThetaLight/distSqr = 0.25
    // weight = lightPDF/(lightPDF+brdfPDF_area) = 2.0/2.25 = 8/9
    // contribution = gTerm/lightPDF * weight = 0.25/2.0 * 8/9 = 1/9 ≈ 0.1111
    // If brdfPDF was NOT converted (measure mismatch): weight = 2/(2+1) = 2/3 → contribution ≈ 0.0833
    FixedBSDF emissiveBSDF(WHITE); emissiveBSDF.emission = WHITE;
    FixedBSDF shadingBSDF(WHITE);  // PDF=1.0 solid angle
    Triangle lightTri = makeLightTriangle();
    BackgroundColour blackBg(BLACK);
    Scene scene; Film film;
    scene.init({lightTri}, {&emissiveBSDF}, &blackBg);
    scene.build();
    scene.width = 64; scene.height = 64; film.init(64, 64);
    Integrator integrator(&scene, &film);
    ShadingData sd = makeSD(&shadingBSDF);
    FixedSampler sampler{FS_SEL, FS_R1, FS_R2};

    Colour result = BLACK;
    integrator.lightSamplingMIS(sd, &sampler, result);

    float brdfPDF_area = 1.0f * BASE_COS_THETA_L / BASE_DIST_SQR;         // = 0.25
    float weight       = BASE_LIGHT_PDF / (BASE_LIGHT_PDF + brdfPDF_area); // = 8/9
    float expected     = (BASE_G_TERM / BASE_LIGHT_PDF) * weight;          // = 1/9
    REQUIRE(result.r == Catch::Approx(expected).margin(1e-4f));
    REQUIRE(result.g == Catch::Approx(expected).margin(1e-4f));
    REQUIRE(result.b == Catch::Approx(expected).margin(1e-4f));
}

TEST_CASE("lightSamplingMIS area light: result accumulates — two calls with same sampler sequence add up") {
    // FixedSampler wraps after 3 values so the second call sees identical values → same contribution.
    FixedBSDF emissiveBSDF(WHITE); emissiveBSDF.emission = WHITE;
    ZeroPDFBSDF shadingBSDF(WHITE);
    Triangle lightTri = makeLightTriangle();
    BackgroundColour blackBg(BLACK);
    Scene scene; Film film;
    scene.init({lightTri}, {&emissiveBSDF}, &blackBg);
    scene.build();
    scene.width = 64; scene.height = 64; film.init(64, 64);
    Integrator integrator(&scene, &film);
    ShadingData sd = makeSD(&shadingBSDF);

    Colour single = BLACK;
    FixedSampler s1{FS_SEL, FS_R1, FS_R2};
    integrator.lightSamplingMIS(sd, &s1, single);

    Colour doubled = BLACK;
    FixedSampler s2{FS_SEL, FS_R1, FS_R2};
    integrator.lightSamplingMIS(sd, &s2, doubled);
    integrator.lightSamplingMIS(sd, &s2, doubled);  // s2 wraps → identical values again

    REQUIRE(doubled.r == Catch::Approx(single.r * 2.0f).margin(1e-4f));
    REQUIRE(doubled.g == Catch::Approx(single.g * 2.0f).margin(1e-4f));
    REQUIRE(doubled.b == Catch::Approx(single.b * 2.0f).margin(1e-4f));
}

TEST_CASE("lightSamplingMIS area light: PMF is included in light PDF — two equal lights give pmf=0.5") {
    // Two identical triangles (area=0.5, WHITE emission) → equal power → pmf=0.5 each.
    // FixedSampler 0.3 forces selection of light[0]; r1=0.25, r2=0.5 → (0.25,2,0.25).
    // gTerm=0.25, fullPdf = pmf*pdf_area = 0.5*2.0 = 1.0, ZeroPDFBSDF → weight=1.
    // contribution = gTerm/fullPdf = 0.25/1.0 = 0.25
    // Without PMF: fullPdf=2.0 → contribution = 0.125 — wrong.
    FixedBSDF emissiveBSDF(WHITE); emissiveBSDF.emission = WHITE;
    ZeroPDFBSDF shadingBSDF(WHITE);
    Triangle lightTri1 = makeLightTriangle();
    Triangle lightTri2 = makeSecondLightTriangle();
    BackgroundColour blackBg(BLACK);
    Scene scene; Film film;
    scene.init({lightTri1, lightTri2}, {&emissiveBSDF}, &blackBg);
    scene.build();
    scene.width = 64; scene.height = 64; film.init(64, 64);
    Integrator integrator(&scene, &film);
    ShadingData sd = makeSD(&shadingBSDF);
    // 0.3 * totalPower(=2*0.5*π=π) ≈ 0.94 < cumulative after light[0] (0.5π≈1.57) → selects light[0], pmf=0.5
    FixedSampler sampler{0.3f, FS_R1, FS_R2};

    Colour result = BLACK;
    integrator.lightSamplingMIS(sd, &sampler, result);

    // pmf=0.5 → fullPdf=1.0 → contribution = 0.25/1.0 * 1.0 = 0.25
    REQUIRE(result.r == Catch::Approx(0.25f).margin(1e-4f));
}

TEST_CASE("lightSamplingMIS env map: result is positive for valid unoccluded configuration") {
    ZeroPDFBSDF shadingBSDF(WHITE);
    FixedBSDF dummyBSDF(BLACK);
    Triangle dummy = makeDistantTriangle();
    MockEnvLight mockEnv(Vec3(0,1,0), WHITE, 0.5f);
    Scene scene; Film film;
    scene.init({dummy}, {&dummyBSDF}, &mockEnv);
    scene.build();
    scene.width = 64; scene.height = 64; film.init(64, 64);
    Integrator integrator(&scene, &film);
    ShadingData sd = makeSD(&shadingBSDF);
    FixedSampler sampler{0.5f};

    Colour result = BLACK;
    integrator.lightSamplingMIS(sd, &sampler, result);
    REQUIRE(result.r > 0.f);
}

TEST_CASE("lightSamplingMIS env map: result is black when path is occluded") {
    ZeroPDFBSDF shadingBSDF(WHITE);
    FixedBSDF blockerBSDF(BLACK);
    Triangle blocker = makeBlockerTriangle();
    MockEnvLight mockEnv(Vec3(0,1,0), WHITE, 0.5f);
    Scene scene; Film film;
    scene.init({blocker}, {&blockerBSDF}, &mockEnv);
    scene.build();
    scene.width = 64; scene.height = 64; film.init(64, 64);
    Integrator integrator(&scene, &film);
    ShadingData sd = makeSD(&shadingBSDF);
    FixedSampler sampler{0.5f};

    Colour result = BLACK;
    integrator.lightSamplingMIS(sd, &sampler, result);
    REQUIRE(result.r == Catch::Approx(0.f).margin(1e-5f));
    REQUIRE(result.g == Catch::Approx(0.f).margin(1e-5f));
    REQUIRE(result.b == Catch::Approx(0.f).margin(1e-5f));
}

TEST_CASE("lightSamplingMIS env map: analytical — weight=1 when brdfPDF=0, contribution=emission*cosTheta/envPDF") {
    // MockEnvLight: wi=(0,1,0), emission=WHITE, pdf=0.5; pmf=1.0 (only light).
    // ZeroPDFBSDF: brdfPDF=0; cosThetaLine=1, distSqr=1 sentinels → brdfPDF*1/1 = 0.
    // weight = 0.5/(0.5+0) = 1; contribution = finalColor/envPDF = (WHITE*WHITE*1.0)/0.5 = 2.0
    ZeroPDFBSDF shadingBSDF(WHITE);
    FixedBSDF dummyBSDF(BLACK);
    Triangle dummy = makeDistantTriangle();
    MockEnvLight mockEnv(Vec3(0,1,0), WHITE, 0.5f);
    Scene scene; Film film;
    scene.init({dummy}, {&dummyBSDF}, &mockEnv);
    scene.build();
    scene.width = 64; scene.height = 64; film.init(64, 64);
    Integrator integrator(&scene, &film);
    ShadingData sd = makeSD(&shadingBSDF);
    FixedSampler sampler{0.5f};

    Colour result = BLACK;
    integrator.lightSamplingMIS(sd, &sampler, result);

    // bsdf·emission·cosTheta / envPDF = 1·1·1 / 0.5 = 2.0
    REQUIRE(result.r == Catch::Approx(2.0f).margin(1e-4f));
    REQUIRE(result.g == Catch::Approx(2.0f).margin(1e-4f));
    REQUIRE(result.b == Catch::Approx(2.0f).margin(1e-4f));
}

// -----------------------------------------------------------------------
// computeDirectMIS tests
//
// computeDirectMIS adds one guard over lightSamplingMIS:
//   if (bsdf->isPureSpecular()) return BLACK
// Otherwise it delegates to lightSamplingMIS and returns its result.
// -----------------------------------------------------------------------

// BSDF that reports itself as pure specular.
class PureSpecularBSDF : public BSDF {
public:
    Vec3   sample(const ShadingData&, Sampler*, Colour& w, float& pdf) override { w = WHITE; pdf = 1.f; return Vec3(0,1,0); }
    Colour evaluate(const ShadingData&, const Vec3&) override { return WHITE; }
    float  PDF(const ShadingData&, const Vec3&) override { return 1.f; }
    bool   isPureSpecular() override { return true; }
    bool   isTwoSided() override { return true; }
    float  mask(const ShadingData&) override { return 1.f; }
};

TEST_CASE("computeDirectMIS: returns black for pure specular BSDF regardless of lights") {
    // Pure specular surfaces cannot receive direct light via shadow rays.
    PureSpecularBSDF specBSDF;
    FixedBSDF emissiveBSDF(WHITE); emissiveBSDF.emission = WHITE;
    Triangle lightTri = makeLightTriangle();
    BackgroundColour blackBg(BLACK);
    Scene scene; Film film;
    scene.init({lightTri}, {&emissiveBSDF}, &blackBg);
    scene.build();
    scene.width = 64; scene.height = 64; film.init(64, 64);
    Integrator integrator(&scene, &film);
    ShadingData sd = makeSD(&specBSDF);
    MTRandom sampler;

    Colour result = integrator.computeDirectMIS(sd, &sampler);
    REQUIRE(result.r == Catch::Approx(0.f).margin(1e-5f));
    REQUIRE(result.g == Catch::Approx(0.f).margin(1e-5f));
    REQUIRE(result.b == Catch::Approx(0.f).margin(1e-5f));
}

TEST_CASE("computeDirectMIS: returns positive result for diffuse BSDF with area light") {
    FixedBSDF emissiveBSDF(WHITE); emissiveBSDF.emission = WHITE;
    Triangle lightTri = makeLightTriangle();
    BackgroundColour blackBg(BLACK);
    Scene scene; Film film;
    scene.init({lightTri}, {&emissiveBSDF}, &blackBg);
    scene.build();
    scene.width = 64; scene.height = 64; film.init(64, 64);
    Integrator integrator(&scene, &film);
    ShadingData sd = makeSD(&emissiveBSDF);
    MTRandom sampler;

    Colour result = integrator.computeDirectMIS(sd, &sampler);
    REQUIRE(result.r > 0.f);
}

TEST_CASE("computeDirectMIS: returns black when light is occluded") {
    FixedBSDF emissiveBSDF(WHITE); emissiveBSDF.emission = WHITE;
    FixedBSDF blockerBSDF(BLACK);
    Triangle lightTri = makeLightTriangle();
    Triangle blocker   = makeBlockerTriangle();
    blocker.materialIndex = 1;
    BackgroundColour blackBg(BLACK);
    Scene scene; Film film;
    scene.init({lightTri, blocker}, {&emissiveBSDF, &blockerBSDF}, &blackBg);
    scene.build();
    scene.width = 64; scene.height = 64; film.init(64, 64);
    Integrator integrator(&scene, &film);
    ShadingData sd = makeSD(&emissiveBSDF);
    MTRandom sampler;

    Colour result = integrator.computeDirectMIS(sd, &sampler);
    REQUIRE(result.r == Catch::Approx(0.f).margin(1e-5f));
    REQUIRE(result.g == Catch::Approx(0.f).margin(1e-5f));
    REQUIRE(result.b == Catch::Approx(0.f).margin(1e-5f));
}

TEST_CASE("computeDirectMIS: analytical — matches lightSamplingMIS result for identical setup") {
    // Verifies delegation is numerically exact: same scene + same sampler → same value.
    // ZeroPDFBSDF + FixedSampler{0.5,0.25,0.5} → weight=1 → contribution=0.125 (as in lightSamplingMIS tests)
    FixedBSDF emissiveBSDF(WHITE); emissiveBSDF.emission = WHITE;
    ZeroPDFBSDF shadingBSDF(WHITE);
    Triangle lightTri = makeLightTriangle();
    BackgroundColour blackBg(BLACK);
    Scene scene; Film film;
    scene.init({lightTri}, {&emissiveBSDF}, &blackBg);
    scene.build();
    scene.width = 64; scene.height = 64; film.init(64, 64);
    Integrator integrator(&scene, &film);
    ShadingData sd = makeSD(&shadingBSDF);
    FixedSampler sampler{FS_SEL, FS_R1, FS_R2};

    Colour result = integrator.computeDirectMIS(sd, &sampler);
    REQUIRE(result.r == Catch::Approx(0.125f).margin(1e-4f));
    REQUIRE(result.g == Catch::Approx(0.125f).margin(1e-4f));
    REQUIRE(result.b == Catch::Approx(0.125f).margin(1e-4f));
}

TEST_CASE("computeDirectMIS: returns positive result for diffuse BSDF with env map") {
    ZeroPDFBSDF shadingBSDF(WHITE);
    FixedBSDF dummyBSDF(BLACK);
    Triangle dummy = makeDistantTriangle();
    MockEnvLight mockEnv(Vec3(0,1,0), WHITE, 0.5f);
    Scene scene; Film film;
    scene.init({dummy}, {&dummyBSDF}, &mockEnv);
    scene.build();
    scene.width = 64; scene.height = 64; film.init(64, 64);
    Integrator integrator(&scene, &film);
    ShadingData sd = makeSD(&shadingBSDF);
    FixedSampler sampler{0.5f};

    Colour result = integrator.computeDirectMIS(sd, &sampler);
    REQUIRE(result.r > 0.f);
}

TEST_CASE("lightSamplingMIS env map: analytical — PDFs stay in solid angle via cosThetaLine=distSqr=1 sentinels") {
    // FixedBSDF PDF=1.0 (solid angle). Sentinels make brdfPDF * 1/1 = 1.0 (no conversion).
    // weight = envPDF/(envPDF+brdfPDF_sa) = 0.5/(0.5+1.0) = 1/3
    // contribution = finalColor/envPDF * weight = (1/0.5) * (1/3) = 2/3 ≈ 0.6667
    // Wrong without sentinels: using real geometry values for cosThetaLine/distSqr would give different result.
    FixedBSDF shadingBSDF(WHITE);   // PDF=1.0
    FixedBSDF dummyBSDF(BLACK);
    Triangle dummy = makeDistantTriangle();
    MockEnvLight mockEnv(Vec3(0,1,0), WHITE, 0.5f);
    Scene scene; Film film;
    scene.init({dummy}, {&dummyBSDF}, &mockEnv);
    scene.build();
    scene.width = 64; scene.height = 64; film.init(64, 64);
    Integrator integrator(&scene, &film);
    ShadingData sd = makeSD(&shadingBSDF);
    FixedSampler sampler{0.5f};

    Colour result = BLACK;
    integrator.lightSamplingMIS(sd, &sampler, result);

    // brdfPDF_sa=1.0, weight=0.5/(0.5+1.0)=1/3, contribution=(1/0.5)*(1/3)=2/3
    float expected = (1.0f / 0.5f) * (0.5f / (0.5f + 1.0f));
    REQUIRE(result.r == Catch::Approx(expected).margin(1e-4f));
    REQUIRE(result.g == Catch::Approx(expected).margin(1e-4f));
    REQUIRE(result.b == Catch::Approx(expected).margin(1e-4f));
}
