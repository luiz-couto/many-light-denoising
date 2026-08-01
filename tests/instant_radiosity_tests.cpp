#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cfloat>
#include <cmath>
#include "instant_radiosity.h"
#include "shading.h"
#include "texture.h"
#include "light.h"
#include "config.h"

// -----------------------------------------------------------------------
// Contracts these tests encode (implementation must follow):
//
// 1. emitPhoton returns the SINGLE-photon flux — WITHOUT the 1/numPaths
//    division. prepare() applies 1/IR_NUM_LIGHT_PATHS exactly once when it
//    initialises the path flux. Rationale: emitPhoton stays independent of
//    the config constant, and the "divide by N exactly once" invariant has
//    one owner (prepare).
// 2. emitPhoton fills emittedRay with origin ON the light (offset along the
//    travel direction is allowed) and direction = the photon TRAVEL
//    direction (unit length), for every light type. For EnvironmentMap this
//    means flipping the sampled toward-the-environment direction.
// 3. Le is read through the light interface as evaluate(-travelDirection) —
//    "look back at the light". Flux is a Colour end to end (no luminance
//    collapse).
// 4. EnvironmentMap emission treats the scene bounding sphere as the
//    emitting surface: cosEmit = dot(travelDir, inwardSphereNormal),
//    clamped to >= 0 (outward photons get zero flux — discarded unbiasedly).
// 5. depositVPL: radiance = flux * albedo(tu,tv) / PI, position = sd.x,
//    normal = sd.sNormal. No numPaths factor here.
// 6. gatherVPLs: sum over vpls of
//    evaluate(sd, wi) * radiance * min(G, IR_G_CLAMP) * visible — no
//    division by vpls.size() or numPaths (already inside radiance).
// 7. prepare() CLEARS the previous pass's VPLs before tracing new ones.
// -----------------------------------------------------------------------

// Sampler that cycles a fixed sequence — same pattern as path_tracer_tests.
class FixedSampler : public Sampler {
  std::vector<float> values;
  size_t idx = 0;
public:
  explicit FixedSampler(std::initializer_list<float> v) : values(v) {}
  float next() override { float v = values[idx % values.size()]; ++idx; return v; }
};

// Uniform-colour texture (pattern from environment_map_tests.cpp).
static void fillTexture(Texture& tex, int w, int h, Colour c) {
  tex.width    = w;
  tex.height   = h;
  tex.channels = 3;
  tex.texels   = new Colour[w * h];
  tex.alpha    = nullptr;
  for (int i = 0; i < w * h; i++) tex.texels[i] = c;
}

// Triangle with one shared normal for all three vertices.
static Triangle makeTri(Vec3 v0, Vec3 v1, Vec3 v2, Vec3 normal, unsigned int matIdx) {
  Triangle tri;
  tri.init(Vertex(v0, normal, 0.0f, 0.0f),
           Vertex(v1, normal, 1.0f, 0.0f),
           Vertex(v2, normal, 0.0f, 1.0f), matIdx);
  return tri;
}

// -----------------------------------------------------------------------
// emitPhoton — area light
// -----------------------------------------------------------------------

// One emissive triangle (area 2, facing +z), black zero-power background.
// For a Lambertian area light with cosine direction sampling, the emission
// cosine cancels against the direction pdf:
//   flux = Le * cos / (pmf * posPdf * dirPdf)
//        = Le * cos / (1 * (1/area) * (cos/PI))
//        = Le * PI * area
// — independent of WHICH direction was sampled. So the flux value is exact
// for every draw, with any sampler.
TEST_CASE("InstantRadiosity emitPhoton: area light flux equals emission * PI * area / pmf") {
  const Colour emission(2.0f, 1.0f, 0.5f);
  Texture white; fillTexture(white, 1, 1, Colour(1.0f, 1.0f, 1.0f));
  DiffuseBSDF lightMaterial(&white);
  lightMaterial.addLight(emission);

  // Right triangle, legs 2 and 2 -> area = 2, geometric normal +z.
  Triangle lightTri = makeTri(Vec3(0.0f, 0.0f, 0.0f), Vec3(2.0f, 0.0f, 0.0f),
                              Vec3(0.0f, 2.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f), 0);
  const float area = 2.0f;

  BackgroundColour background(Colour(0.0f, 0.0f, 0.0f)); // zero power, not a light
  Scene scene;
  scene.init({lightTri}, {&lightMaterial}, &background);
  scene.build();
  REQUIRE(scene.lights.size() == 1); // pmf = 1

  Film film;
  film.init(8, 8);
  InstantRadiosityIntegrator integrator(&scene, &film);

  MTRandom sampler(42);
  const Colour expectedFlux = emission * PI * area;

  for (int i = 0; i < 100; i++) {
    Ray emittedRay;
    Colour flux = integrator.emitPhoton(&sampler, emittedRay);

    REQUIRE(flux.r == Catch::Approx(expectedFlux.r).epsilon(1e-3f));
    REQUIRE(flux.g == Catch::Approx(expectedFlux.g).epsilon(1e-3f));
    REQUIRE(flux.b == Catch::Approx(expectedFlux.b).epsilon(1e-3f));

    // Origin on the light's plane (z = 0), inside the triangle bounds.
    REQUIRE(std::abs(emittedRay.o.z) < 1e-3f);
    REQUIRE(emittedRay.o.x >= -1e-4f);
    REQUIRE(emittedRay.o.y >= -1e-4f);
    REQUIRE(emittedRay.o.x + emittedRay.o.y <= 2.0f + 1e-3f);

    // Travel direction: unit length, leaving the light's front face (+z).
    REQUIRE(emittedRay.dir.length() == Catch::Approx(1.0f).margin(1e-4f));
    REQUIRE(emittedRay.dir.z > 0.0f);
  }
}

// Regression for the old path tracer's grayscale bug (Renderer.h:75-76):
// flux must preserve the emission's colour RATIOS, not collapse to luminance.
TEST_CASE("InstantRadiosity emitPhoton: flux preserves emission colour ratios") {
  const Colour emission(4.0f, 0.2f, 0.02f);
  Texture white; fillTexture(white, 1, 1, Colour(1.0f, 1.0f, 1.0f));
  DiffuseBSDF lightMaterial(&white);
  lightMaterial.addLight(emission);

  Triangle lightTri = makeTri(Vec3(0.0f, 0.0f, 0.0f), Vec3(1.0f, 0.0f, 0.0f),
                              Vec3(0.0f, 1.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f), 0);
  BackgroundColour background(Colour(0.0f, 0.0f, 0.0f));
  Scene scene;
  scene.init({lightTri}, {&lightMaterial}, &background);
  scene.build();

  Film film;
  film.init(8, 8);
  InstantRadiosityIntegrator integrator(&scene, &film);

  MTRandom sampler(7);
  Ray emittedRay;
  Colour flux = integrator.emitPhoton(&sampler, emittedRay);

  REQUIRE(flux.r / flux.g == Catch::Approx(emission.r / emission.g).epsilon(1e-3f));
  REQUIRE(flux.g / flux.b == Catch::Approx(emission.g / emission.b).epsilon(1e-3f));
}

// Two lights with different powers: the selection pmf must divide the flux.
// FixedSampler's first value 0.0 forces sampleLightWeighted to pick the
// first light in scene.lights; its pmf = P1 / (P1 + P2).
TEST_CASE("InstantRadiosity emitPhoton: two lights — flux divided by selection pmf") {
  const Colour emissionBig(3.0f, 3.0f, 3.0f);   // area 2   -> P1 = 2 * lum * PI
  const Colour emissionSmall(1.0f, 1.0f, 1.0f); // area 0.5 -> P2 = 0.5 * lum * PI
  Texture white; fillTexture(white, 1, 1, Colour(1.0f, 1.0f, 1.0f));
  DiffuseBSDF bigMaterial(&white);   bigMaterial.addLight(emissionBig);
  DiffuseBSDF smallMaterial(&white); smallMaterial.addLight(emissionSmall);

  Triangle bigTri   = makeTri(Vec3(0.0f, 0.0f, 0.0f), Vec3(2.0f, 0.0f, 0.0f),
                              Vec3(0.0f, 2.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f), 0);
  Triangle smallTri = makeTri(Vec3(5.0f, 0.0f, 0.0f), Vec3(6.0f, 0.0f, 0.0f),
                              Vec3(5.0f, 1.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f), 1);
  const float bigArea = 2.0f;

  BackgroundColour background(Colour(0.0f, 0.0f, 0.0f));
  Scene scene;
  scene.init({bigTri, smallTri}, {&bigMaterial, &smallMaterial}, &background);
  scene.build();
  REQUIRE(scene.lights.size() == 2);

  float powerBig   = bigArea * emissionBig.lum() * PI;
  float powerSmall = 0.5f * emissionSmall.lum() * PI;
  float pmfBig     = powerBig / (powerBig + powerSmall);

  Film film;
  film.init(8, 8);
  InstantRadiosityIntegrator integrator(&scene, &film);

  // First value 0.0 -> first light; remaining values feed position/direction.
  FixedSampler sampler{0.0f, 0.3f, 0.3f, 0.3f, 0.3f};
  Ray emittedRay;
  Colour flux = integrator.emitPhoton(&sampler, emittedRay);

  const Colour expectedFlux = emissionBig * PI * bigArea / pmfBig;
  REQUIRE(flux.r == Catch::Approx(expectedFlux.r).epsilon(1e-3f));
  REQUIRE(flux.g == Catch::Approx(expectedFlux.g).epsilon(1e-3f));
  REQUIRE(flux.b == Catch::Approx(expectedFlux.b).epsilon(1e-3f));
}

// -----------------------------------------------------------------------
// emitPhoton — environment map
// -----------------------------------------------------------------------

// Contract test for the env emission conventions (the trickiest corner):
//   - ray origin lies ON the scene bounding sphere
//   - ray direction is the TRAVEL direction (flipped from the sampled
//     toward-the-environment direction)
//   - flux = Le * cosEmit / (pmf * posPdf * dirPdf), with
//     cosEmit = max(0, dot(travelDir, inwardNormal)) measured on the sphere
//   - outward photons (cosEmit = 0) carry zero flux — discarded unbiasedly
//
// A uniform white env makes every pdf analytically computable:
//   posPdf   = 1 / (4 * PI * R^2)                       (uniform sphere)
//   dirPdf   = W*H / (totalSum * 2 * PI^2)               (uniform texels)
//   totalSum = sum over rows u of W * sin(PI * u / H)    (buildCDF weights)
TEST_CASE("InstantRadiosity emitPhoton: environment map — sphere origin, inward flux identity") {
  Texture envTex; fillTexture(envTex, 4, 4, Colour(1.0f, 1.0f, 1.0f));

  const Vec3  sceneCentre(0.0f, 0.0f, 0.0f);
  const float sceneRadius = 10.0f;
  EnvironmentMap environment(&envTex, sceneCentre, sceneRadius);

  // One non-emissive floor triangle so the scene has geometry/materials.
  Texture white; fillTexture(white, 1, 1, Colour(1.0f, 1.0f, 1.0f));
  DiffuseBSDF floorMaterial(&white);
  Triangle floorTri = makeTri(Vec3(-1.0f, -1.0f, 0.0f), Vec3(1.0f, -1.0f, 0.0f),
                              Vec3(-1.0f, 1.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f), 0);

  Scene scene;
  scene.init({floorTri}, {&floorMaterial}, &environment);
  scene.build();
  REQUIRE(scene.lights.size() == 1); // env map only -> pmf = 1

  Film film;
  film.init(8, 8);
  InstantRadiosityIntegrator integrator(&scene, &film);

  // Analytical pdfs for the uniform 4x4 map.
  const float posPdf = 1.0f / (4.0f * PI * sceneRadius * sceneRadius);
  float totalSum = 0.0f;
  for (int u = 0; u < 4; u++) totalSum += 4.0f * sinf(PI * (float)u / 4.0f);
  const float dirPdf = 16.0f / (totalSum * 2.0f * PI * PI);

  MTRandom sampler(42);
  int nonZeroCount = 0;

  for (int i = 0; i < 200; i++) {
    Ray emittedRay;
    Colour flux = integrator.emitPhoton(&sampler, emittedRay);

    // Origin on the bounding sphere.
    REQUIRE((emittedRay.o - sceneCentre).length() == Catch::Approx(sceneRadius).epsilon(1e-3f));

    Vec3 inwardNormal = (sceneCentre - emittedRay.o).normalize();
    float cosEmit = dot(emittedRay.dir, inwardNormal);

    REQUIRE(std::isfinite(flux.r));
    REQUIRE(flux.r >= 0.0f);

    if (flux.lum() > 0.0f) {
      nonZeroCount++;
      REQUIRE(cosEmit > 0.0f); // non-zero flux only for inward photons
      // Le = white (uniform map), so the identity is purely geometric:
      float expected = std::max(cosEmit, 0.0f) / (posPdf * dirPdf);
      REQUIRE(flux.r == Catch::Approx(expected).epsilon(1e-3f));
      REQUIRE(flux.g == Catch::Approx(expected).epsilon(1e-3f));
      REQUIRE(flux.b == Catch::Approx(expected).epsilon(1e-3f));
    }
  }

  // Roughly half the (position, direction) pairs point inward; with 200
  // draws, at least SOME must carry flux or emission is broken.
  REQUIRE(nonZeroCount > 20);
}

// -----------------------------------------------------------------------
// depositVPL
// -----------------------------------------------------------------------

TEST_CASE("InstantRadiosity depositVPL: radiance = flux * albedo / PI, fields copied") {
  const Colour albedoColour(0.5f, 0.25f, 1.0f);
  Texture albedoTex; fillTexture(albedoTex, 1, 1, albedoColour);
  DiffuseBSDF floorMaterial(&albedoTex);

  Scene scene; // not used by depositVPL, but the integrator needs one
  Film film;
  film.init(8, 8);
  InstantRadiosityIntegrator integrator(&scene, &film);

  ShadingData shadingData(Vec3(1.0f, 2.0f, 3.0f), Vec3(0.0f, 0.0f, 1.0f));
  shadingData.bsdf = &floorMaterial;
  shadingData.tu = 0.0f;
  shadingData.tv = 0.0f;

  const Colour flux(2.0f, 4.0f, 6.0f);
  integrator.depositVPL(shadingData, flux, albedoColour);

  REQUIRE(integrator.vpls.size() == 1);
  const VPL& vpl = integrator.vpls[0];

  REQUIRE(vpl.position.x == Catch::Approx(1.0f));
  REQUIRE(vpl.position.y == Catch::Approx(2.0f));
  REQUIRE(vpl.position.z == Catch::Approx(3.0f));
  REQUIRE(vpl.normal.z   == Catch::Approx(1.0f));

  REQUIRE(vpl.radiance.r == Catch::Approx(flux.r * albedoColour.r / PI).epsilon(1e-4f));
  REQUIRE(vpl.radiance.g == Catch::Approx(flux.g * albedoColour.g / PI).epsilon(1e-4f));
  REQUIRE(vpl.radiance.b == Catch::Approx(flux.b * albedoColour.b / PI).epsilon(1e-4f));
}

// -----------------------------------------------------------------------
// gatherVPLs — analytical single-VPL cases
// -----------------------------------------------------------------------

// White diffuse floor at z=0 covering the origin; shading point at (0,0,0).
static Scene makeFloorScene(DiffuseBSDF* floorMaterial, BackgroundColour* background) {
  Triangle floorTri = makeTri(Vec3(-10.0f, -10.0f, 0.0f), Vec3(10.0f, -10.0f, 0.0f),
                              Vec3(0.0f, 10.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f), 0);
  Scene scene;
  scene.init({floorTri}, {floorMaterial}, background);
  scene.build();
  return scene;
}

static ShadingData shadeFloorOrigin(Scene& scene) {
  Ray ray(Vec3(0.0f, 0.0f, 3.0f), Vec3(0.0f, 0.0f, -1.0f));
  IntersectionData intersection = scene.traverse(ray);
  REQUIRE(intersection.t < FLT_MAX);
  return scene.calculateShadingData(intersection, ray);
}

// VPL 2 units directly above, facing straight down:
//   wi = (0,0,1), cos_x = 1, cos_vpl = 1, distSqr = 4 -> G = 0.25 (below clamp)
//   contribution = (albedo/PI) * radiance * G per channel.
// COLOURED radiance on purpose: a missing vpl.radiance multiply passes
// silently with white radiance — with (2, 0.5, 0.25) it fails loudly.
TEST_CASE("InstantRadiosity gatherVPLs: single VPL analytical contribution") {
  Texture white; fillTexture(white, 1, 1, Colour(1.0f, 1.0f, 1.0f));
  DiffuseBSDF floorMaterial(&white);
  BackgroundColour background(Colour(0.0f, 0.0f, 0.0f));
  Scene scene = makeFloorScene(&floorMaterial, &background);

  Film film;
  film.init(8, 8);
  InstantRadiosityIntegrator integrator(&scene, &film);

  VPL vpl;
  vpl.position = Vec3(0.0f, 0.0f, 2.0f);
  vpl.normal   = Vec3(0.0f, 0.0f, -1.0f);
  vpl.radiance = Colour(2.0f, 0.5f, 0.25f);
  integrator.vpls.push_back(vpl);

  ShadingData shadingData = shadeFloorOrigin(scene);
  Colour result = integrator.gatherVPLs(shadingData);

  const float geometry = 0.25f / PI; // G * albedo/PI with white albedo
  REQUIRE(result.r == Catch::Approx(vpl.radiance.r * geometry).epsilon(1e-3f));
  REQUIRE(result.g == Catch::Approx(vpl.radiance.g * geometry).epsilon(1e-3f));
  REQUIRE(result.b == Catch::Approx(vpl.radiance.b * geometry).epsilon(1e-3f));
}

// VPL epsilon-close: G = 1/0.0001 = 10000 -> clamped to IR_G_CLAMP.
// Without the clamp this test fails with a value 1000x too large.
TEST_CASE("InstantRadiosity gatherVPLs: near-singular VPL clamped to IR_G_CLAMP") {
  Texture white; fillTexture(white, 1, 1, Colour(1.0f, 1.0f, 1.0f));
  DiffuseBSDF floorMaterial(&white);
  BackgroundColour background(Colour(0.0f, 0.0f, 0.0f));
  Scene scene = makeFloorScene(&floorMaterial, &background);

  Film film;
  film.init(8, 8);
  InstantRadiosityIntegrator integrator(&scene, &film);

  VPL vpl;
  vpl.position = Vec3(0.0f, 0.0f, 0.01f);
  vpl.normal   = Vec3(0.0f, 0.0f, -1.0f);
  vpl.radiance = Colour(1.0f, 1.0f, 1.0f);
  integrator.vpls.push_back(vpl);

  ShadingData shadingData = shadeFloorOrigin(scene);
  Colour result = integrator.gatherVPLs(shadingData);

  const float expected = Config::IR_G_CLAMP / PI;
  REQUIRE(result.r == Catch::Approx(expected).epsilon(1e-3f));
}

// VPL above, small occluder triangle in between: contribution must be zero.
TEST_CASE("InstantRadiosity gatherVPLs: occluded VPL contributes zero") {
  Texture white; fillTexture(white, 1, 1, Colour(1.0f, 1.0f, 1.0f));
  DiffuseBSDF floorMaterial(&white);
  DiffuseBSDF occluderMaterial(&white);
  BackgroundColour background(Colour(0.0f, 0.0f, 0.0f));

  Triangle floorTri    = makeTri(Vec3(-10.0f, -10.0f, 0.0f), Vec3(10.0f, -10.0f, 0.0f),
                                 Vec3(0.0f, 10.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f), 0);
  // Small patch at z=1 covering only the segment (0,0,0) -> (0,0,2).
  Triangle occluderTri = makeTri(Vec3(-0.5f, -0.5f, 1.0f), Vec3(0.5f, -0.5f, 1.0f),
                                 Vec3(0.0f, 0.5f, 1.0f), Vec3(0.0f, 0.0f, 1.0f), 1);

  Scene scene;
  scene.init({floorTri, occluderTri}, {&floorMaterial, &occluderMaterial}, &background);
  scene.build();

  Film film;
  film.init(8, 8);
  InstantRadiosityIntegrator integrator(&scene, &film);

  VPL vpl;
  vpl.position = Vec3(0.0f, 0.0f, 2.0f);
  vpl.normal   = Vec3(0.0f, 0.0f, -1.0f);
  vpl.radiance = Colour(1.0f, 1.0f, 1.0f);
  integrator.vpls.push_back(vpl);

  // Shading point at the floor origin, reached from an angle that misses
  // the occluder: ray from (3,3,3) toward the origin passes z=1 at (1,1,1).
  Ray ray(Vec3(3.0f, 3.0f, 3.0f), Vec3(-1.0f, -1.0f, -1.0f).normalize());
  IntersectionData intersection = scene.traverse(ray);
  REQUIRE(intersection.t < FLT_MAX);
  ShadingData shadingData = scene.calculateShadingData(intersection, ray);

  Colour result = integrator.gatherVPLs(shadingData);
  REQUIRE(result.r == Catch::Approx(0.0f).margin(1e-6f));
  REQUIRE(result.g == Catch::Approx(0.0f).margin(1e-6f));
  REQUIRE(result.b == Catch::Approx(0.0f).margin(1e-6f));
}

// -----------------------------------------------------------------------
// prepare — invariants (full pass over a lit diffuse floor)
// -----------------------------------------------------------------------

// Small area light at z=2 facing down, big diffuse floor at z=0.
// Cosine-sampled photons travel downward and must deposit on the floor.
TEST_CASE("InstantRadiosity prepare: deposits VPLs, clears between passes, deterministic per seed") {
  const Colour emission(5.0f, 5.0f, 5.0f);
  Texture white; fillTexture(white, 1, 1, Colour(1.0f, 1.0f, 1.0f));
  DiffuseBSDF lightMaterial(&white);
  lightMaterial.addLight(emission);
  DiffuseBSDF floorMaterial(&white);

  // Vertex normals (0,0,-1) orient gNormal downward regardless of winding.
  Triangle lightTri = makeTri(Vec3(-0.5f, -0.5f, 2.0f), Vec3(0.5f, -0.5f, 2.0f),
                              Vec3(-0.5f, 0.5f, 2.0f), Vec3(0.0f, 0.0f, -1.0f), 0);
  Triangle floorTri = makeTri(Vec3(-10.0f, -10.0f, 0.0f), Vec3(10.0f, -10.0f, 0.0f),
                              Vec3(0.0f, 10.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f), 1);

  BackgroundColour background(Colour(0.0f, 0.0f, 0.0f));
  Scene scene;
  scene.init({lightTri, floorTri}, {&lightMaterial, &floorMaterial}, &background);
  scene.build();

  Film film;
  film.init(8, 8);
  InstantRadiosityIntegrator integrator(&scene, &film);

  MTRandom samplerA(42);
  integrator.prepare(&samplerA);

  REQUIRE(integrator.vpls.size() >= 1);
  REQUIRE(integrator.vpls.size() <= (size_t)(Config::IR_NUM_LIGHT_PATHS * Config::IR_MAX_PHOTON_DEPTH));

  int floorCount = 0;
  for (const VPL& vpl : integrator.vpls) {
    REQUIRE(std::isfinite(vpl.radiance.r));
    REQUIRE(std::isfinite(vpl.radiance.g));
    REQUIRE(std::isfinite(vpl.radiance.b));
    REQUIRE(vpl.radiance.r >= 0.0f);
    REQUIRE(vpl.radiance.g >= 0.0f);
    REQUIRE(vpl.radiance.b >= 0.0f);
    if (std::abs(vpl.position.z) < 1e-3f) floorCount++;
  }
  REQUIRE(floorCount >= 1); // photons from the downward light must reach the floor

  size_t firstPassCount = integrator.vpls.size();
  Vec3 firstPassPosition = integrator.vpls[0].position;

  // Same seed again: prepare must CLEAR and reproduce the identical set.
  MTRandom samplerB(42);
  integrator.prepare(&samplerB);
  REQUIRE(integrator.vpls.size() == firstPassCount);
  REQUIRE(integrator.vpls[0].position.x == Catch::Approx(firstPassPosition.x));
  REQUIRE(integrator.vpls[0].position.y == Catch::Approx(firstPassPosition.y));
  REQUIRE(integrator.vpls[0].position.z == Catch::Approx(firstPassPosition.z));

  // Different seed: a fresh VPL world (some position must differ).
  MTRandom samplerC(1234);
  integrator.prepare(&samplerC);
  bool anyDifferent = integrator.vpls.empty() ||
                      integrator.vpls[0].position.x != firstPassPosition.x ||
                      integrator.vpls[0].position.y != firstPassPosition.y;
  REQUIRE(anyDifferent);
}

// -----------------------------------------------------------------------
// integrate — depth-0 behaviour matches the path tracer
// -----------------------------------------------------------------------

TEST_CASE("InstantRadiosity integrate: miss returns background colour") {
  const Colour backgroundColour(0.25f, 0.5f, 0.75f);
  Texture white; fillTexture(white, 1, 1, Colour(1.0f, 1.0f, 1.0f));
  DiffuseBSDF floorMaterial(&white);
  BackgroundColour background(backgroundColour);
  Scene scene = makeFloorScene(&floorMaterial, &background);

  Film film;
  film.init(8, 8);
  InstantRadiosityIntegrator integrator(&scene, &film);

  // Ray pointing away from all geometry.
  Ray ray(Vec3(0.0f, 0.0f, 3.0f), Vec3(0.0f, 0.0f, 1.0f));
  MTRandom sampler(42);
  Colour result = integrator.integrate(ray, &sampler);

  REQUIRE(result.r == Catch::Approx(backgroundColour.r).margin(1e-5f));
  REQUIRE(result.g == Catch::Approx(backgroundColour.g).margin(1e-5f));
  REQUIRE(result.b == Catch::Approx(backgroundColour.b).margin(1e-5f));
}

// =======================================================================
// SECOND WAVE — transport-chain math, edge cases, cross-validation
// =======================================================================

#include "path_tracer.h"

// -----------------------------------------------------------------------
// Photon transport conservation — THE chain test.
//
// Two large parallel diffuse planes (albedo a = 0.5) at z=0 and z=4, a
// tiny downward light between them. Every photon ping-pongs between the
// planes, depositing at every hit, until Russian roulette or the depth
// cap ends it. Expected TOTAL deposited radiance per pass (all photons):
//
//   E[sum radiance] = fluxTotal/PI * (a + a^2 + a^3 + a^4)
//                   = emission * area * a * (1 + a + a^2 + a^3)
//
// (fluxTotal = emission*PI*area; deposit d carries flux*a^d, radiance
// multiplies a/PI; RR survivors divide by q so expectations hold.)
//
// This single number is sensitive to: the missing-RR-division bug (-12%),
// deposit-after-RR ordering, a doubled or missing 1/numPaths, and any
// error in the bounce weight. The light's albedo is BLACK so photons that
// wander back into it deposit nothing (keeps the series exact).
// -----------------------------------------------------------------------
TEST_CASE("InstantRadiosity prepare: total deposited radiance matches the bounce series (RR unbiased)") {
  const Colour emission(8.0f, 6.0f, 4.0f);
  const float albedoValue = 0.5f;
  const float lightArea = 0.02f; // right triangle, legs 0.2

  Texture black; fillTexture(black, 1, 1, Colour(0.0f, 0.0f, 0.0f));
  Texture gray;  fillTexture(gray, 1, 1, Colour(albedoValue, albedoValue, albedoValue));
  DiffuseBSDF lightMaterial(&black);   // black albedo: emits but never receives deposits
  lightMaterial.addLight(emission);
  DiffuseBSDF planeMaterial(&gray);

  Triangle lightTri   = makeTri(Vec3(-0.1f, -0.1f, 2.0f), Vec3(0.1f, -0.1f, 2.0f),
                                Vec3(-0.1f, 0.1f, 2.0f), Vec3(0.0f, 0.0f, -1.0f), 0);
  Triangle floorTri   = makeTri(Vec3(-200.0f, -200.0f, 0.0f), Vec3(200.0f, -200.0f, 0.0f),
                                Vec3(0.0f, 200.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f), 1);
  Triangle ceilingTri = makeTri(Vec3(-200.0f, -200.0f, 4.0f), Vec3(200.0f, -200.0f, 4.0f),
                                Vec3(0.0f, 200.0f, 4.0f), Vec3(0.0f, 0.0f, -1.0f), 1);

  BackgroundColour background(Colour(0.0f, 0.0f, 0.0f));
  Scene scene;
  scene.init({lightTri, floorTri, ceilingTri}, {&lightMaterial, &planeMaterial}, &background);
  scene.build();

  Film film;
  film.init(8, 8);
  InstantRadiosityIntegrator integrator(&scene, &film);

  const float series = albedoValue * (1.0f + albedoValue + albedoValue * albedoValue
                                      + albedoValue * albedoValue * albedoValue); // 0.9375
  const Colour expectedTotal = emission * lightArea * series;

  const int numRuns = 30;
  Colour averagedTotal(0.0f, 0.0f, 0.0f);
  for (int run = 0; run < numRuns; run++) {
    MTRandom sampler(run + 1);
    integrator.prepare(&sampler);
    for (const VPL& vpl : integrator.vpls) averagedTotal = averagedTotal + vpl.radiance;
  }
  averagedTotal = averagedTotal / (float)numRuns;

  REQUIRE(averagedTotal.r == Catch::Approx(expectedTotal.r).epsilon(0.05f));
  REQUIRE(averagedTotal.g == Catch::Approx(expectedTotal.g).epsilon(0.05f));
  REQUIRE(averagedTotal.b == Catch::Approx(expectedTotal.b).epsilon(0.05f));
}

// -----------------------------------------------------------------------
// Environment emission integrates to the correct total power.
// For uniform white Le = 1 over a bounding sphere of radius R:
//   E[flux] = integral over sphere area x all directions of Le * cosEmit
//           = (4*PI) * (PI * R^2) = 4 * PI^2 * R^2
// Validates cosEmit + posPdf + dirPdf jointly, including the outward
// discard (zero-flux draws COUNT in the mean — same rule as
// estimateReflectance in bsdf_test_utils.h).
// -----------------------------------------------------------------------
TEST_CASE("InstantRadiosity emitPhoton: env emission mean flux matches the discrete CDF power") {
  Texture envTex; fillTexture(envTex, 4, 4, Colour(1.0f, 1.0f, 1.0f));
  const Vec3  sceneCentre(0.0f, 0.0f, 0.0f);
  const float sceneRadius = 10.0f;
  EnvironmentMap environment(&envTex, sceneCentre, sceneRadius);

  Texture white; fillTexture(white, 1, 1, Colour(1.0f, 1.0f, 1.0f));
  DiffuseBSDF floorMaterial(&white);
  Triangle floorTri = makeTri(Vec3(-1.0f, -1.0f, 0.0f), Vec3(1.0f, -1.0f, 0.0f),
                              Vec3(-1.0f, 1.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f), 0);
  Scene scene;
  scene.init({floorTri}, {&floorMaterial}, &environment);
  scene.build();

  Film film;
  film.init(8, 8);
  InstantRadiosityIntegrator integrator(&scene, &film);

  MTRandom sampler(42);
  const int numPhotons = 20000;
  double sum = 0.0;
  for (int i = 0; i < numPhotons; i++) {
    Ray emittedRay;
    Colour flux = integrator.emitPhoton(&sampler, emittedRay);
    sum += flux.r;   // zero-flux (outward) draws included — they carry the measure
  }
  float mean = (float)(sum / numPhotons);

  // Expected value for the DISCRETE 4x4 CDF, not the continuum limit:
  // sampleDirectionFromLight draws from the 16 texel-corner directions and
  // its pdf normalizes by the same Riemann sum, so the direction integral is
  //   integral Le domega (discrete) = (2*PI^2 / (W*H)) * sum of lum*sin(PI*u/H)
  // E[flux] = PI*R^2 * that sum. (The continuum 4*PI^2*R^2 holds only as
  // the map resolution grows — a 4x4 map sits ~5% below it.)
  float totalSum = 0.0f;
  for (int u = 0; u < 4; u++) totalSum += 4.0f * sinf(PI * (float)u / 4.0f);
  const float discreteOmega = 2.0f * PI * PI * totalSum / 16.0f;
  const float expected = PI * sceneRadius * sceneRadius * discreteOmega;

  REQUIRE(mean == Catch::Approx(expected).epsilon(0.03f));
}

// -----------------------------------------------------------------------
// Deposit predicate: mirror/glass/conductor floors receive NO VPLs
// (photon continues through them); plastic floor DOES receive VPLs.
// -----------------------------------------------------------------------
static size_t countVPLsOnFloor(BSDF* floorMaterial) {
  const Colour emission(5.0f, 5.0f, 5.0f);
  Texture black; fillTexture(black, 1, 1, Colour(0.0f, 0.0f, 0.0f));
  DiffuseBSDF lightMaterial(&black);
  lightMaterial.addLight(emission);

  Triangle lightTri = makeTri(Vec3(-0.5f, -0.5f, 2.0f), Vec3(0.5f, -0.5f, 2.0f),
                              Vec3(-0.5f, 0.5f, 2.0f), Vec3(0.0f, 0.0f, -1.0f), 0);
  Triangle floorTri = makeTri(Vec3(-50.0f, -50.0f, 0.0f), Vec3(50.0f, -50.0f, 0.0f),
                              Vec3(0.0f, 50.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f), 1);

  BackgroundColour background(Colour(0.0f, 0.0f, 0.0f));
  Scene scene;
  scene.init({lightTri, floorTri}, {&lightMaterial, floorMaterial}, &background);
  scene.build();

  Film film;
  film.init(8, 8);
  InstantRadiosityIntegrator integrator(&scene, &film);
  MTRandom sampler(42);
  integrator.prepare(&sampler);
  return integrator.vpls.size();
}

TEST_CASE("InstantRadiosity prepare: no deposits on mirror, glass, or conductor; deposits on plastic") {
  Texture white; fillTexture(white, 1, 1, Colour(1.0f, 1.0f, 1.0f));
  const Colour silverEta(0.177f, 0.178f, 0.172f);
  const Colour silverK(3.638f, 2.973f, 2.430f);

  MirrorBSDF    mirrorFloor(&white, silverEta, silverK);
  GlassBSDF     glassFloor(&white, 1.5f, 1.0f);
  ConductorBSDF conductorFloor(&white, silverEta, silverK, 0.3f);
  PlasticBSDF   plasticFloor(&white, 1.5f, 1.0f, 0.3f);

  REQUIRE(countVPLsOnFloor(&mirrorFloor) == 0);
  REQUIRE(countVPLsOnFloor(&glassFloor) == 0);
  REQUIRE(countVPLsOnFloor(&conductorFloor) == 0);
  REQUIRE(countVPLsOnFloor(&plasticFloor) > 0);
}

// -----------------------------------------------------------------------
// gatherVPLs — geometric edge cases
// -----------------------------------------------------------------------

TEST_CASE("InstantRadiosity gatherVPLs: VPL behind the shading surface contributes zero") {
  Texture white; fillTexture(white, 1, 1, Colour(1.0f, 1.0f, 1.0f));
  DiffuseBSDF floorMaterial(&white);
  BackgroundColour background(Colour(0.0f, 0.0f, 0.0f));
  Scene scene = makeFloorScene(&floorMaterial, &background);

  Film film; film.init(8, 8);
  InstantRadiosityIntegrator integrator(&scene, &film);

  VPL vpl;
  vpl.position = Vec3(0.0f, 0.0f, -2.0f);  // below the floor: cos_x <= 0
  vpl.normal   = Vec3(0.0f, 0.0f, 1.0f);
  vpl.radiance = Colour(1.0f, 1.0f, 1.0f);
  integrator.vpls.push_back(vpl);

  Colour result = integrator.gatherVPLs(shadeFloorOrigin(scene));
  REQUIRE(result.r == Catch::Approx(0.0f).margin(1e-6f));
}

TEST_CASE("InstantRadiosity gatherVPLs: VPL facing away contributes zero") {
  Texture white; fillTexture(white, 1, 1, Colour(1.0f, 1.0f, 1.0f));
  DiffuseBSDF floorMaterial(&white);
  BackgroundColour background(Colour(0.0f, 0.0f, 0.0f));
  Scene scene = makeFloorScene(&floorMaterial, &background);

  Film film; film.init(8, 8);
  InstantRadiosityIntegrator integrator(&scene, &film);

  VPL vpl;
  vpl.position = Vec3(0.0f, 0.0f, 2.0f);
  vpl.normal   = Vec3(0.0f, 0.0f, 1.0f);   // glowing side points AWAY from the floor
  vpl.radiance = Colour(1.0f, 1.0f, 1.0f);
  integrator.vpls.push_back(vpl);

  Colour result = integrator.gatherVPLs(shadeFloorOrigin(scene));
  REQUIRE(result.r == Catch::Approx(0.0f).margin(1e-6f));
}

TEST_CASE("InstantRadiosity gatherVPLs: VPL coincident with the shading point — finite, zero") {
  Texture white; fillTexture(white, 1, 1, Colour(1.0f, 1.0f, 1.0f));
  DiffuseBSDF floorMaterial(&white);
  BackgroundColour background(Colour(0.0f, 0.0f, 0.0f));
  Scene scene = makeFloorScene(&floorMaterial, &background);

  Film film; film.init(8, 8);
  InstantRadiosityIntegrator integrator(&scene, &film);

  ShadingData shadingData = shadeFloorOrigin(scene);
  VPL vpl;
  vpl.position = shadingData.x;             // distSqr = 0: normalize would NaN
  vpl.normal   = Vec3(0.0f, 0.0f, 1.0f);
  vpl.radiance = Colour(1.0f, 1.0f, 1.0f);
  integrator.vpls.push_back(vpl);

  Colour result = integrator.gatherVPLs(shadingData);
  REQUIRE(std::isfinite(result.r));
  REQUIRE(result.r == Catch::Approx(0.0f).margin(1e-6f));
}

TEST_CASE("InstantRadiosity gatherVPLs: two identical VPLs contribute exactly twice one") {
  Texture white; fillTexture(white, 1, 1, Colour(1.0f, 1.0f, 1.0f));
  DiffuseBSDF floorMaterial(&white);
  BackgroundColour background(Colour(0.0f, 0.0f, 0.0f));
  Scene scene = makeFloorScene(&floorMaterial, &background);

  Film film; film.init(8, 8);
  InstantRadiosityIntegrator integrator(&scene, &film);

  VPL vpl;
  vpl.position = Vec3(0.0f, 0.0f, 2.0f);
  vpl.normal   = Vec3(0.0f, 0.0f, -1.0f);
  vpl.radiance = Colour(1.0f, 1.0f, 1.0f);
  integrator.vpls.push_back(vpl);
  integrator.vpls.push_back(vpl);

  Colour result = integrator.gatherVPLs(shadeFloorOrigin(scene));
  REQUIRE(result.r == Catch::Approx(2.0f * 0.25f / PI).epsilon(1e-3f));
}

// Off-axis geometry: VPL at (0,2,2) facing down, shading point at origin.
//   wi = (0,1,1)/sqrt(2), cos_x = 1/sqrt(2), cos_vpl = 1/sqrt(2),
//   distSqr = 8 -> G = 0.5/8 = 0.0625; contribution = 0.0625/PI.
// Catches sign/normalization errors that axis-aligned setups can mask.
TEST_CASE("InstantRadiosity gatherVPLs: tilted VPL analytical contribution") {
  Texture white; fillTexture(white, 1, 1, Colour(1.0f, 1.0f, 1.0f));
  DiffuseBSDF floorMaterial(&white);
  BackgroundColour background(Colour(0.0f, 0.0f, 0.0f));
  Scene scene = makeFloorScene(&floorMaterial, &background);

  Film film; film.init(8, 8);
  InstantRadiosityIntegrator integrator(&scene, &film);

  VPL vpl;
  vpl.position = Vec3(0.0f, 2.0f, 2.0f);
  vpl.normal   = Vec3(0.0f, 0.0f, -1.0f);
  vpl.radiance = Colour(1.0f, 1.0f, 1.0f);
  integrator.vpls.push_back(vpl);

  Colour result = integrator.gatherVPLs(shadeFloorOrigin(scene));
  REQUIRE(result.r == Catch::Approx(0.0625f / PI).epsilon(1e-3f));
}

// -----------------------------------------------------------------------
// integrate — remaining branches
// -----------------------------------------------------------------------

TEST_CASE("InstantRadiosity integrate: emissive hit returns emission exactly") {
  const Colour emission(3.0f, 2.0f, 1.0f);
  Texture black; fillTexture(black, 1, 1, Colour(0.0f, 0.0f, 0.0f));
  DiffuseBSDF lightMaterial(&black);
  lightMaterial.addLight(emission);

  Triangle lightTri = makeTri(Vec3(-1.0f, -1.0f, 2.0f), Vec3(1.0f, -1.0f, 2.0f),
                              Vec3(-1.0f, 1.0f, 2.0f), Vec3(0.0f, 0.0f, -1.0f), 0);
  BackgroundColour background(Colour(0.0f, 0.0f, 0.0f));
  Scene scene;
  scene.init({lightTri}, {&lightMaterial}, &background);
  scene.build();

  Film film; film.init(8, 8);
  InstantRadiosityIntegrator integrator(&scene, &film);

  Ray ray(Vec3(-0.2f, -0.2f, 0.0f), Vec3(0.0f, 0.0f, 1.0f));
  MTRandom sampler(42);
  Colour result = integrator.integrate(ray, &sampler);

  REQUIRE(result.r == Catch::Approx(emission.r).margin(1e-5f));
  REQUIRE(result.g == Catch::Approx(emission.g).margin(1e-5f));
  REQUIRE(result.b == Catch::Approx(emission.b).margin(1e-5f));
}

// Infinite mirror corridor: two parallel mirrors, camera ray bouncing
// between them forever. Must terminate (depth cap) and return black —
// not hang, not NaN.
TEST_CASE("InstantRadiosity integrate: mirror corridor terminates black at the specular depth cap") {
  Texture white; fillTexture(white, 1, 1, Colour(1.0f, 1.0f, 1.0f));
  const Colour silverEta(0.177f, 0.178f, 0.172f);
  const Colour silverK(3.638f, 2.973f, 2.430f);
  MirrorBSDF bottomMirror(&white, silverEta, silverK);
  MirrorBSDF topMirror(&white, silverEta, silverK);

  Triangle bottomTri = makeTri(Vec3(-10.0f, -10.0f, 0.0f), Vec3(10.0f, -10.0f, 0.0f),
                               Vec3(0.0f, 10.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f), 0);
  Triangle topTri    = makeTri(Vec3(-10.0f, -10.0f, 2.0f), Vec3(10.0f, -10.0f, 2.0f),
                               Vec3(0.0f, 10.0f, 2.0f), Vec3(0.0f, 0.0f, -1.0f), 1);

  BackgroundColour background(Colour(0.9f, 0.9f, 0.9f)); // bright bg: leakage would show
  Scene scene;
  scene.init({bottomTri, topTri}, {&bottomMirror, &topMirror}, &background);
  scene.build();

  Film film; film.init(8, 8);
  InstantRadiosityIntegrator integrator(&scene, &film);

  Ray ray(Vec3(0.1f, 0.1f, 1.0f), Vec3(0.0f, 0.0f, -1.0f));
  MTRandom sampler(42);
  Colour result = integrator.integrate(ray, &sampler);

  REQUIRE(std::isfinite(result.r));
  REQUIRE(result.r == Catch::Approx(0.0f).margin(1e-6f));
  REQUIRE(result.g == Catch::Approx(0.0f).margin(1e-6f));
  REQUIRE(result.b == Catch::Approx(0.0f).margin(1e-6f));
}

// Difference test: integrate() with vs without one hand-placed VPL, using
// IDENTICAL FixedSamplers. gatherVPLs consumes no random numbers, so the
// direct-lighting term cancels exactly and the difference must equal the
// analytical gather contribution. Verifies integrate = direct + gather
// with throughput 1 at depth 0.
TEST_CASE("InstantRadiosity integrate: adding a VPL adds exactly its gather contribution") {
  const Colour emission(5.0f, 5.0f, 5.0f);
  Texture black; fillTexture(black, 1, 1, Colour(0.0f, 0.0f, 0.0f));
  Texture white; fillTexture(white, 1, 1, Colour(1.0f, 1.0f, 1.0f));
  DiffuseBSDF lightMaterial(&black);
  lightMaterial.addLight(emission);
  DiffuseBSDF floorMaterial(&white);

  Triangle lightTri = makeTri(Vec3(2.5f, -0.5f, 3.0f), Vec3(3.5f, -0.5f, 3.0f),
                              Vec3(2.5f, 0.5f, 3.0f), Vec3(0.0f, 0.0f, -1.0f), 0);
  Triangle floorTri = makeTri(Vec3(-10.0f, -10.0f, 0.0f), Vec3(10.0f, -10.0f, 0.0f),
                              Vec3(0.0f, 10.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f), 1);
  BackgroundColour background(Colour(0.0f, 0.0f, 0.0f));
  Scene scene;
  scene.init({lightTri, floorTri}, {&lightMaterial, &floorMaterial}, &background);
  scene.build();

  Film film; film.init(8, 8);
  InstantRadiosityIntegrator integrator(&scene, &film);

  Ray ray(Vec3(0.0f, 0.0f, 3.0f), Vec3(0.0f, 0.0f, -1.0f));

  FixedSampler samplerA{0.1f, 0.4f, 0.4f};
  Colour withoutVPL = integrator.integrate(ray, &samplerA);

  VPL vpl;
  vpl.position = Vec3(0.0f, 0.0f, 2.0f);
  vpl.normal   = Vec3(0.0f, 0.0f, -1.0f);
  vpl.radiance = Colour(2.0f, 0.5f, 0.25f);
  integrator.vpls.push_back(vpl);

  FixedSampler samplerB{0.1f, 0.4f, 0.4f};   // identical stream: direct term cancels
  Colour withVPL = integrator.integrate(ray, &samplerB);

  const float geometry = 0.25f / PI;
  Colour difference = withVPL - withoutVPL;
  REQUIRE(difference.r == Catch::Approx(vpl.radiance.r * geometry).epsilon(1e-3f));
  REQUIRE(difference.g == Catch::Approx(vpl.radiance.g * geometry).epsilon(1e-3f));
  REQUIRE(difference.b == Catch::Approx(vpl.radiance.b * geometry).epsilon(1e-3f));
}

// -----------------------------------------------------------------------
// Cross-validation: IR converges to the path tracer on a diffuse scene.
// Floor + back wall + small area light; the wall receives direct light and
// its VPLs illuminate the floor (one genuine indirect bounce). Mean image
// brightness must agree within a documented tolerance covering the G-clamp
// bias and the differing depth caps (PT MAX_DEPTH=5 vs photons 4).
// -----------------------------------------------------------------------
static float meanLuminance(const Film& film) {
  float sum = 0.0f;
  for (const Colour& pixel : film.film) sum += pixel.lum();
  return sum / ((float)(film.width * film.height) * (float)film.SPP);
}

TEST_CASE("InstantRadiosity vs PathTracer: mean brightness agrees on a diffuse scene") {
  const int W = 24, H = 24;
  const Colour emission(20.0f, 20.0f, 20.0f);
  Texture black; fillTexture(black, 1, 1, Colour(0.0f, 0.0f, 0.0f));
  Texture white; fillTexture(white, 1, 1, Colour(1.0f, 1.0f, 1.0f));
  DiffuseBSDF lightMaterial(&black);
  lightMaterial.addLight(emission);
  DiffuseBSDF floorMaterial(&white);
  DiffuseBSDF wallMaterial(&white);

  // y-up: floor at y=0, wall behind the scene at z=-2, light at y=2 facing down.
  Triangle floorTri = makeTri(Vec3(-20.0f, 0.0f, -20.0f), Vec3(20.0f, 0.0f, -20.0f),
                              Vec3(0.0f, 0.0f, 20.0f), Vec3(0.0f, 1.0f, 0.0f), 1);
  Triangle wallTri  = makeTri(Vec3(-8.0f, 0.0f, -2.0f), Vec3(8.0f, 0.0f, -2.0f),
                              Vec3(0.0f, 8.0f, -2.0f), Vec3(0.0f, 0.0f, 1.0f), 2);
  Triangle lightTri = makeTri(Vec3(-0.5f, 2.0f, -0.5f), Vec3(0.5f, 2.0f, -0.5f),
                              Vec3(-0.5f, 2.0f, 0.5f), Vec3(0.0f, -1.0f, 0.0f), 0);

  BackgroundColour background(Colour(0.0f, 0.0f, 0.0f));
  Scene scene;
  scene.init({lightTri, floorTri, wallTri}, {&lightMaterial, &floorMaterial, &wallMaterial}, &background);
  scene.build();
  scene.width  = W;
  scene.height = H;

  float aspect = (float)W / (float)H;
  Matrix P = Matrix::perspective(0.001f, 1000.0f, aspect, 45.0f);
  Matrix V = Matrix::lookAt(Vec3(0.0f, 3.0f, 5.0f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f)).invert();
  scene.camera.init(P, W, H);
  scene.camera.updateView(V);

  Film filmPT; filmPT.init(W, H);
  PathTracerIntegrator pathTracer(&scene, &filmPT);
  for (int i = 0; i < 64; i++) pathTracer.render();

  Film filmIR; filmIR.init(W, H);
  InstantRadiosityIntegrator instantRadiosity(&scene, &filmIR);
  for (int i = 0; i < 16; i++) instantRadiosity.render();

  float meanPT = meanLuminance(filmPT);
  float meanIR = meanLuminance(filmIR);
  REQUIRE(meanPT > 0.0f);
  float ratio = meanIR / meanPT;

  // Documented tolerance: G-clamp bias (darkens IR), depth-cap mismatch,
  // MC noise at these sample counts. Agreement outside this band means a
  // real energy bug (missing pdf, doubled 1/N, RR division...).
  REQUIRE(ratio > 0.8f);
  REQUIRE(ratio < 1.2f);
}
