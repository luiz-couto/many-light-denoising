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
