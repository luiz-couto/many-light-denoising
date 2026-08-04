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

// -----------------------------------------------------------------------
// renderTile auxiliary buffer tests (filmNormals / filmAlbedos)
//
// Camera at (0,0,5) looking at origin, 45° FOV, 64×64.
// Large triangle at z=0 with normal=(0,0,1) covers the entire frustum —
// every pixel center ray hits it regardless of jitter.
// -----------------------------------------------------------------------

static constexpr int AUX_W = 64;
static constexpr int AUX_H = 64;

class AuxWhiteBSDF : public BSDF {
public:
    Vec3   sample(const ShadingData&, Sampler*, Colour& w, float& pdf) override { w = WHITE; pdf = 1.0f; return Vec3(0.0f, 0.0f, 1.0f); }
    Colour evaluate(const ShadingData&, const Vec3&) override { return WHITE; }
    float  PDF(const ShadingData&, const Vec3&) override { return 1.0f; }
    bool   isPureSpecular() override { return false; }
    bool   isTwoSided() override { return true; }
    float  mask(const ShadingData&) override { return 1.0f; }
};

static void setupAuxScene(Scene& scene, Film& film, AuxWhiteBSDF& bsdf, BackgroundColour& bg) {
    Vertex v0(Vec3(-10.0f, -10.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f), 0.0f, 0.0f);
    Vertex v1(Vec3( 10.0f, -10.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f), 1.0f, 0.0f);
    Vertex v2(Vec3(  0.0f,  10.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f), 0.5f, 1.0f);
    Triangle tri; tri.init(v0, v1, v2, 0);

    scene.init({tri}, {&bsdf}, &bg);
    scene.build();
    scene.width  = AUX_W;
    scene.height = AUX_H;
    film.init(AUX_W, AUX_H);

    float aspect = (float)AUX_W / (float)AUX_H;
    Matrix P = Matrix::perspective(0.001f, 1000.0f, aspect, 45.0f);
    Matrix V = Matrix::lookAt(Vec3(0.0f, 0.0f, 5.0f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f)).invert();
    scene.camera.init(P, AUX_W, AUX_H);
    scene.camera.updateView(V);
}

TEST_CASE("renderTile: filmNormals is zero at center pixel before first render") {
    AuxWhiteBSDF bsdf; BackgroundColour bg(BLACK);
    Scene scene; Film film;
    setupAuxScene(scene, film, bsdf, bg);

    int cx = AUX_W / 2;
    int cy = AUX_H / 2;
    Colour normal = film.filmNormals[cy * film.width + cx];
    REQUIRE(normal.r == Catch::Approx(0.0f).margin(1e-5f));
    REQUIRE(normal.g == Catch::Approx(0.0f).margin(1e-5f));
    REQUIRE(normal.b == Catch::Approx(0.0f).margin(1e-5f));
}

TEST_CASE("renderTile: filmNormals populated at center pixel after first render") {
    // Triangle normal = (0,0,1) → filmNormals.b ≈ 1 at center pixel.
    AuxWhiteBSDF bsdf; BackgroundColour bg(BLACK);
    Scene scene; Film film;
    setupAuxScene(scene, film, bsdf, bg);
    Integrator integrator(&scene, &film);

    integrator.render();

    int cx = AUX_W / 2;
    int cy = AUX_H / 2;
    Colour normal = film.filmNormals[cy * film.width + cx];
    REQUIRE(normal.b > 0.5f);
}

TEST_CASE("renderTile: filmAlbedos populated at center pixel after first render") {
    // AuxWhiteBSDF evaluate returns WHITE → filmAlbedos should be (1,1,1) at center.
    AuxWhiteBSDF bsdf; BackgroundColour bg(BLACK);
    Scene scene; Film film;
    setupAuxScene(scene, film, bsdf, bg);
    Integrator integrator(&scene, &film);

    integrator.render();

    int cx = AUX_W / 2;
    int cy = AUX_H / 2;
    Colour albedo = film.filmAlbedos[cy * film.width + cx];
    REQUIRE(albedo.r == Catch::Approx(1.0f).margin(1e-5f));
    REQUIRE(albedo.g == Catch::Approx(1.0f).margin(1e-5f));
    REQUIRE(albedo.b == Catch::Approx(1.0f).margin(1e-5f));
}

TEST_CASE("renderTile: filmNormals not overwritten on subsequent renders") {
    // SPP==0 guard: the second render() call must not touch filmNormals.
    AuxWhiteBSDF bsdf; BackgroundColour bg(BLACK);
    Scene scene; Film film;
    setupAuxScene(scene, film, bsdf, bg);
    Integrator integrator(&scene, &film);

    integrator.render();
    int cx = AUX_W / 2;
    int cy = AUX_H / 2;
    Colour normalAfter1 = film.filmNormals[cy * film.width + cx];

    integrator.render();
    Colour normalAfter2 = film.filmNormals[cy * film.width + cx];

    REQUIRE(normalAfter2.r == Catch::Approx(normalAfter1.r).margin(1e-6f));
    REQUIRE(normalAfter2.g == Catch::Approx(normalAfter1.g).margin(1e-6f));
    REQUIRE(normalAfter2.b == Catch::Approx(normalAfter1.b).margin(1e-6f));
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

// -----------------------------------------------------------------------
// MIS weight sum = 1 (balance heuristic)
//
// For any direction sampled by the light, both the light-sampling weight and
// the BSDF-sampling weight (evaluated at that same direction) must sum to 1.
//
// Analytical base case (FS_R1=0.25, FS_R2=0.5 → sampled point (0.25,2,0.25)):
//   lightPDF_area = 2.0,  distSqr = 4.0,  cosThetaLine = 1.0
//
//   bsdfPDF_sa=1.0:  bsdfPDF_area=0.25  → w_light=2/(2+0.25)=8/9
//                    lightPDF_sa =8.0   → w_bsdf =1/(1+8)  =1/9   sum=1
//
//   bsdfPDF_sa=4.0:  bsdfPDF_area=1.0   → w_light=2/(2+1)=2/3
//                    lightPDF_sa =8.0   → w_bsdf =4/(4+8)=1/3   sum=1
// -----------------------------------------------------------------------

TEST_CASE("MIS weights: light weight plus BSDF weight equals 1.0 for area light") {
    FixedBSDF emissiveBSDF(WHITE); emissiveBSDF.emission = WHITE;
    BackgroundColour blackBg(BLACK);
    Triangle lightTri = makeLightTriangle();
    AreaLight light; light.triangle = &lightTri; light.emission = WHITE;
    Scene scene; Film film;
    scene.init({lightTri}, {&emissiveBSDF}, &blackBg);
    scene.build();
    scene.width = 64; scene.height = 64; film.init(64, 64);
    Integrator integrator(&scene, &film);
    ShadingData sd = makeSD(&emissiveBSDF);

    // lightSamplingMISAreaLight does not consume a selector value — no FS_SEL.
    FixedSampler lightSampler{FS_R1, FS_R2};
    LightSamplingMISResult misRes = integrator.lightSamplingMISAreaLight(sd, &lightSampler, &light, 1.0f);

    float bsdfPDF_sa   = 1.0f;  // FixedBSDF.PDF = 1.0
    float bsdfPDF_area = bsdfPDF_sa * misRes.cosThetaLine / misRes.distSqr;
    float w_light      = misRes.pdf / (misRes.pdf + bsdfPDF_area);

    float lightPDF_sa  = misRes.pdf * misRes.distSqr / misRes.cosThetaLine;
    float w_bsdf       = bsdfPDF_sa / (bsdfPDF_sa + lightPDF_sa);

    REQUIRE(w_light + w_bsdf == Catch::Approx(1.0f).margin(1e-4f));
}

TEST_CASE("MIS weights: light plus BSDF weight equals 1.0 at arbitrary bsdfPDF") {
    FixedBSDF emissiveBSDF(WHITE); emissiveBSDF.emission = WHITE;
    BackgroundColour blackBg(BLACK);
    Triangle lightTri = makeLightTriangle();
    AreaLight light; light.triangle = &lightTri; light.emission = WHITE;
    Scene scene; Film film;
    scene.init({lightTri}, {&emissiveBSDF}, &blackBg);
    scene.build();
    scene.width = 64; scene.height = 64; film.init(64, 64);
    Integrator integrator(&scene, &film);
    ShadingData sd = makeSD(&emissiveBSDF);

    FixedSampler lightSampler{FS_R1, FS_R2};
    LightSamplingMISResult misRes = integrator.lightSamplingMISAreaLight(sd, &lightSampler, &light, 1.0f);

    float bsdfPDF_sa   = 4.0f;
    float bsdfPDF_area = bsdfPDF_sa * misRes.cosThetaLine / misRes.distSqr;
    float w_light      = misRes.pdf / (misRes.pdf + bsdfPDF_area);

    float lightPDF_sa  = misRes.pdf * misRes.distSqr / misRes.cosThetaLine;
    float w_bsdf       = bsdfPDF_sa / (bsdfPDF_sa + lightPDF_sa);

    REQUIRE(w_light + w_bsdf == Catch::Approx(1.0f).margin(1e-4f));
}

// -----------------------------------------------------------------------
// Lambertian irradiance formula
//
// A Lambertian floor (evaluate=ρ/π, PDF=0) under a small area light:
//   intensity I  = emission * area / π  =  1 * 0.5 / π
//   irradiance   = ρ * cosTheta * I / r²  =  1 * 1 * (0.5/π) / 4  =  0.125/π
//
// With PDF=0 the MIS weight collapses to 1, so computeDirectMIS returns
// exactly the area-light irradiance formula within 1%.
// -----------------------------------------------------------------------

TEST_CASE("Lambertian irradiance: computeDirectMIS matches albedo * cosTheta * I / r2 within 1%") {
    // ZeroPDFBSDF(1/π): evaluate = ρ/π with ρ=1, PDF = 0 → MIS weight = 1.
    float lambertianEval = 1.0f / (float)M_PI;
    Colour lambertianColour(lambertianEval, lambertianEval, lambertianEval);
    ZeroPDFBSDF floorBSDF(lambertianColour);

    FixedBSDF emissiveBSDF(WHITE); emissiveBSDF.emission = WHITE;
    Triangle lightTri = makeLightTriangle();
    BackgroundColour blackBg(BLACK);
    Scene scene; Film film;
    scene.init({lightTri}, {&emissiveBSDF}, &blackBg);
    scene.build();
    scene.width = 64; scene.height = 64; film.init(64, 64);
    Integrator integrator(&scene, &film);
    ShadingData sd = makeSD(&floorBSDF);
    FixedSampler sampler{FS_SEL, FS_R1, FS_R2};

    Colour result = integrator.computeDirectMIS(sd, &sampler);

    // ρ * cosTheta * (emission * area / π) / r² = 1 * 1 * (1 * 0.5 / π) / 4 = 0.125/π
    float expected = 0.125f / (float)M_PI;
    REQUIRE(result.r == Catch::Approx(expected).margin(expected * 0.01f));
    REQUIRE(result.g == Catch::Approx(expected).margin(expected * 0.01f));
    REQUIRE(result.b == Catch::Approx(expected).margin(expected * 0.01f));
}

// -----------------------------------------------------------------------
// computeDirectBSDFMIS tests (Stage 4b)
//
// The BSDF-sampling half of the direct-light MIS pair. Samples the BSDF once,
// traces one ray:
//   miss          → env radiance * pdf/(pdf + envSelPDF)
//   hit emissive  → emission * pdf/(pdf + areaLightSelectionPDF * distSq/cosThetaLight)
//   hit ordinary  → black (indirect stays the VPLs' job)
// Guards: pure specular → black; pdf<=0 or black throughput → black.
//
// FixedBSDF as the shading BSDF: sample() returns direction (0,1,0), pdf=1,
// throughput=colour. Shading point (0.25,0,0.25) → offset ray origin
// (0.25, RAY_OFFSET_EPSILON, 0.25) going straight up.
// -----------------------------------------------------------------------

// Env light with radiance but zero reported power — never enters scene->lights,
// modelling a background NEE cannot select.
class ZeroPowerEnvLight : public Light {
public:
    Colour emission;
    explicit ZeroPowerEnvLight(Colour e) : emission(e) {}
    Vec3   sample(const ShadingData&, Sampler*, Colour& e, float& pdf) override { e = emission; pdf = 0.5f; return Vec3(0.0f, 1.0f, 0.0f); }
    Colour evaluate(const Vec3&) override { return emission; }
    float  PDF(const ShadingData&, const Vec3&) override { return 0.5f; }
    bool   isArea() override { return false; }
    Vec3   normal(const ShadingData&, const Vec3&) override { return Vec3(0.0f, 1.0f, 0.0f); }
    float  totalIntegratedPower() override { return 0.0f; }
    Vec3   sampleDirectionFromLight(Sampler*, float& pdf) override { pdf = 0.5f; return Vec3(0.0f, 1.0f, 0.0f); }
};

TEST_CASE("computeDirectBSDFMIS: returns black for pure specular BSDF — specular walk owns delta lobes") {
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

    Colour result = integrator.computeDirectBSDFMIS(sd, &sampler);
    REQUIRE(result.r == Catch::Approx(0.0f).margin(1e-6f));
    REQUIRE(result.g == Catch::Approx(0.0f).margin(1e-6f));
    REQUIRE(result.b == Catch::Approx(0.0f).margin(1e-6f));
}

TEST_CASE("computeDirectBSDFMIS: returns black when sample pdf is 0") {
    // ZeroPDFBSDF: sample() returns pdf=0 → guard fires before any traversal.
    ZeroPDFBSDF shadingBSDF(WHITE);
    FixedBSDF emissiveBSDF(WHITE); emissiveBSDF.emission = WHITE;
    Triangle lightTri = makeLightTriangle();
    BackgroundColour blackBg(BLACK);
    Scene scene; Film film;
    scene.init({lightTri}, {&emissiveBSDF}, &blackBg);
    scene.build();
    scene.width = 64; scene.height = 64; film.init(64, 64);
    Integrator integrator(&scene, &film);
    ShadingData sd = makeSD(&shadingBSDF);
    MTRandom sampler;

    Colour result = integrator.computeDirectBSDFMIS(sd, &sampler);
    REQUIRE(result.r == Catch::Approx(0.0f).margin(1e-6f));
    REQUIRE(result.g == Catch::Approx(0.0f).margin(1e-6f));
    REQUIRE(result.b == Catch::Approx(0.0f).margin(1e-6f));
}

TEST_CASE("computeDirectBSDFMIS: returns black when sampled throughput is black") {
    // FixedBSDF(BLACK): pdf=1 but throughput luminance 0 → guard fires.
    FixedBSDF shadingBSDF(BLACK);
    FixedBSDF dummyBSDF(BLACK);
    Triangle dummy = makeDistantTriangle();
    MockEnvLight mockEnv(Vec3(0,1,0), WHITE, 0.5f);
    Scene scene; Film film;
    scene.init({dummy}, {&dummyBSDF}, &mockEnv);
    scene.build();
    scene.width = 64; scene.height = 64; film.init(64, 64);
    Integrator integrator(&scene, &film);
    ShadingData sd = makeSD(&shadingBSDF);
    MTRandom sampler;

    Colour result = integrator.computeDirectBSDFMIS(sd, &sampler);
    REQUIRE(result.r == Catch::Approx(0.0f).margin(1e-6f));
    REQUIRE(result.g == Catch::Approx(0.0f).margin(1e-6f));
    REQUIRE(result.b == Catch::Approx(0.0f).margin(1e-6f));
}

TEST_CASE("computeDirectBSDFMIS: analytical — env miss weighted by pdf/(pdf + envSelPDF)") {
    // Env as only light: envSelPDF = pmf(1.0) * PDF(0.5) = 0.5.
    // pdf=1.0, throughput=WHITE, emission=WHITE → result = 1/(1+0.5) = 2/3.
    // The unweighted version returns 1.0 — this pins the balance weight.
    FixedBSDF shadingBSDF(WHITE);
    FixedBSDF dummyBSDF(BLACK);
    Triangle dummy = makeDistantTriangle();
    MockEnvLight mockEnv(Vec3(0,1,0), WHITE, 0.5f);
    Scene scene; Film film;
    scene.init({dummy}, {&dummyBSDF}, &mockEnv);
    scene.build();
    scene.width = 64; scene.height = 64; film.init(64, 64);
    Integrator integrator(&scene, &film);
    ShadingData sd = makeSD(&shadingBSDF);
    MTRandom sampler;

    Colour result = integrator.computeDirectBSDFMIS(sd, &sampler);

    float expected = 1.0f / (1.0f + 0.5f);  // = 2/3
    REQUIRE(result.r == Catch::Approx(expected).margin(1e-4f));
    REQUIRE(result.g == Catch::Approx(expected).margin(1e-4f));
    REQUIRE(result.b == Catch::Approx(expected).margin(1e-4f));
}

TEST_CASE("computeDirectBSDFMIS: env miss weight includes the light-selection pmf") {
    // lights = [area 0.5π, env 4π] → envSelPDF = (8/9)*0.5 = 4/9.
    // weight = 1/(1 + 4/9) = 9/13 ≈ 0.6923; without pmf: 2/3 ≈ 0.6667 — distinguishable.
    // Second light triangle at x∈[2,3] — the up-ray from (0.25,·,0.25) misses it.
    FixedBSDF shadingBSDF(WHITE);
    FixedBSDF emissiveBSDF(WHITE); emissiveBSDF.emission = WHITE;
    Triangle lightTri = makeSecondLightTriangle();
    MockEnvLight mockEnv(Vec3(0,1,0), WHITE, 0.5f);
    Scene scene; Film film;
    scene.init({lightTri}, {&emissiveBSDF}, &mockEnv);
    scene.build();
    scene.width = 64; scene.height = 64; film.init(64, 64);
    Integrator integrator(&scene, &film);
    ShadingData sd = makeSD(&shadingBSDF);
    MTRandom sampler;

    Colour result = integrator.computeDirectBSDFMIS(sd, &sampler);

    float expected = 1.0f / (1.0f + 4.0f / 9.0f);  // = 9/13
    REQUIRE(result.r == Catch::Approx(expected).margin(1e-4f));
    REQUIRE(result.g == Catch::Approx(expected).margin(1e-4f));
    REQUIRE(result.b == Catch::Approx(expected).margin(1e-4f));
}

TEST_CASE("computeDirectBSDFMIS: zero-power background outside the light list takes full credit") {
    // ZeroPowerEnvLight never enters scene->lights → NEE can never sample it →
    // envSelPDF=0 → weight=1 → result = throughput * emission = 1.0.
    FixedBSDF shadingBSDF(WHITE);
    FixedBSDF dummyBSDF(BLACK);
    Triangle dummy = makeDistantTriangle();
    ZeroPowerEnvLight zeroPowerEnv(WHITE);
    Scene scene; Film film;
    scene.init({dummy}, {&dummyBSDF}, &zeroPowerEnv);
    scene.build();
    scene.width = 64; scene.height = 64; film.init(64, 64);
    Integrator integrator(&scene, &film);
    ShadingData sd = makeSD(&shadingBSDF);
    MTRandom sampler;

    Colour result = integrator.computeDirectBSDFMIS(sd, &sampler);
    REQUIRE(result.r == Catch::Approx(1.0f).margin(1e-4f));
    REQUIRE(result.g == Catch::Approx(1.0f).margin(1e-4f));
    REQUIRE(result.b == Catch::Approx(1.0f).margin(1e-4f));
}

TEST_CASE("computeDirectBSDFMIS: analytical — emissive hit weighted like the pathTrace formula") {
    // Ray from (0.25, ε, 0.25) up hits the light at t = 2-ε → distSq = (2-ε)².
    // cosThetaLight = |(0,-1,0)·(0,-1,0)| = 1. areaLightSelectionPDF = 2.0 (only light).
    // lightPDF_sa = 2 * distSq / 1;  weight = pdf/(pdf + lightPDF_sa) with pdf=1.
    FixedBSDF shadingBSDF(WHITE);
    FixedBSDF emissiveBSDF(WHITE); emissiveBSDF.emission = WHITE;
    Triangle lightTri = makeLightTriangle();
    BackgroundColour blackBg(BLACK);
    Scene scene; Film film;
    scene.init({lightTri}, {&emissiveBSDF}, &blackBg);
    scene.build();
    scene.width = 64; scene.height = 64; film.init(64, 64);
    Integrator integrator(&scene, &film);
    ShadingData sd = makeSD(&shadingBSDF);
    MTRandom sampler;

    Colour result = integrator.computeDirectBSDFMIS(sd, &sampler);

    float t = 2.0f - RAY_OFFSET_EPSILON;
    float lightPDF = 2.0f * t * t;
    float expected = 1.0f / (1.0f + lightPDF);  // ≈ 0.1112
    REQUIRE(result.r == Catch::Approx(expected).margin(1e-4f));
    REQUIRE(result.g == Catch::Approx(expected).margin(1e-4f));
    REQUIRE(result.b == Catch::Approx(expected).margin(1e-4f));
}

TEST_CASE("computeDirectBSDFMIS: emissive hit scales with sampled throughput per channel") {
    // Asymmetric sample throughput exposes a missing multiply: result = colour * weight.
    Colour asymmetric(0.5f, 0.25f, 0.8f);
    FixedBSDF shadingBSDF(asymmetric);
    FixedBSDF emissiveBSDF(WHITE); emissiveBSDF.emission = WHITE;
    Triangle lightTri = makeLightTriangle();
    BackgroundColour blackBg(BLACK);
    Scene scene; Film film;
    scene.init({lightTri}, {&emissiveBSDF}, &blackBg);
    scene.build();
    scene.width = 64; scene.height = 64; film.init(64, 64);
    Integrator integrator(&scene, &film);
    ShadingData sd = makeSD(&shadingBSDF);
    MTRandom sampler;

    Colour result = integrator.computeDirectBSDFMIS(sd, &sampler);

    float t = 2.0f - RAY_OFFSET_EPSILON;
    float weight = 1.0f / (1.0f + 2.0f * t * t);
    REQUIRE(result.r == Catch::Approx(asymmetric.r * weight).margin(1e-4f));
    REQUIRE(result.g == Catch::Approx(asymmetric.g * weight).margin(1e-4f));
    REQUIRE(result.b == Catch::Approx(asymmetric.b * weight).margin(1e-4f));
}

TEST_CASE("computeDirectBSDFMIS: hitting ordinary geometry returns black — no gather, indirect stays the VPLs' job") {
    // Blocker at y=1 intercepts the ray before the light at y=2. Direct-only branch → black.
    FixedBSDF shadingBSDF(WHITE);
    FixedBSDF emissiveBSDF(WHITE); emissiveBSDF.emission = WHITE;
    FixedBSDF blockerBSDF(WHITE);  // reflective but NOT emissive
    Triangle lightTri = makeLightTriangle();
    Triangle blocker  = makeBlockerTriangle();
    blocker.materialIndex = 1;
    BackgroundColour blackBg(BLACK);
    Scene scene; Film film;
    scene.init({lightTri, blocker}, {&emissiveBSDF, &blockerBSDF}, &blackBg);
    scene.build();
    scene.width = 64; scene.height = 64; film.init(64, 64);
    Integrator integrator(&scene, &film);
    ShadingData sd = makeSD(&shadingBSDF);
    MTRandom sampler;

    Colour result = integrator.computeDirectBSDFMIS(sd, &sampler);
    REQUIRE(result.r == Catch::Approx(0.0f).margin(1e-6f));
    REQUIRE(result.g == Catch::Approx(0.0f).margin(1e-6f));
    REQUIRE(result.b == Catch::Approx(0.0f).margin(1e-6f));
}

TEST_CASE("computeDirectBSDFMIS: shading point on real geometry does not self-hit — offset regression") {
    // Shading point ON the blocker surface at (0.25, 1, 0.25). Without the
    // RAY_OFFSET_EPSILON offset the up-ray self-hits the blocker (ordinary
    // geometry → black). With it, the ray reaches the light at t = 1-ε.
    FixedBSDF shadingBSDF(WHITE);
    FixedBSDF emissiveBSDF(WHITE); emissiveBSDF.emission = WHITE;
    FixedBSDF blockerBSDF(WHITE);
    Triangle lightTri = makeLightTriangle();
    Triangle blocker  = makeBlockerTriangle();
    blocker.materialIndex = 1;
    BackgroundColour blackBg(BLACK);
    Scene scene; Film film;
    scene.init({lightTri, blocker}, {&emissiveBSDF, &blockerBSDF}, &blackBg);
    scene.build();
    scene.width = 64; scene.height = 64; film.init(64, 64);
    Integrator integrator(&scene, &film);
    ShadingData sd(Vec3(0.25f, 1.0f, 0.25f), Vec3(0.0f, 1.0f, 0.0f));
    sd.bsdf = &shadingBSDF;
    MTRandom sampler;

    Colour result = integrator.computeDirectBSDFMIS(sd, &sampler);

    float t = 1.0f - RAY_OFFSET_EPSILON;
    float expected = 1.0f / (1.0f + 2.0f * t * t);  // ≈ 0.3338
    REQUIRE(result.r == Catch::Approx(expected).margin(1e-4f));
    REQUIRE(result.g == Catch::Approx(expected).margin(1e-4f));
    REQUIRE(result.b == Catch::Approx(expected).margin(1e-4f));
}
