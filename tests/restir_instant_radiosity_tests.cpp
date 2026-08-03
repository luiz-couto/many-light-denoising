#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <atomic>
#include <cfloat>
#include <cmath>
#include "restir_instant_radiosity.h"
#include "shading.h"
#include "texture.h"
#include "light.h"
#include "config.h"

// Sampler that cycles a fixed sequence — same pattern as the other test files.
class FixedSampler : public Sampler {
  std::vector<float> values;
  size_t idx = 0;
public:
  explicit FixedSampler(std::initializer_list<float> v) : values(v) {}
  float next() override { float v = values[idx % values.size()]; ++idx; return v; }
};

// Tests for the ReSTIR integrator pieces that exist so far:
// runTiled/runPixelFunc, tracePrimary, and pHat. The three phase functions
// (generateCandidates, spatialReuse, shadePixel) get their own tests as they
// are implemented.

// -----------------------------------------------------------------------
// Shared helpers (same patterns as instant_radiosity_tests.cpp)
// -----------------------------------------------------------------------

static void fillTexture(Texture& tex, int w, int h, Colour c) {
  tex.width    = w;
  tex.height   = h;
  tex.channels = 3;
  tex.texels   = new Colour[w * h];
  tex.alpha    = nullptr;
  for (int i = 0; i < w * h; i++) tex.texels[i] = c;
}

static Triangle makeTri(Vec3 v0, Vec3 v1, Vec3 v2, Vec3 normal, unsigned int matIdx) {
  Triangle tri;
  tri.init(Vertex(v0, normal, 0.0f, 0.0f),
           Vertex(v1, normal, 1.0f, 0.0f),
           Vertex(v2, normal, 0.0f, 1.0f), matIdx);
  return tri;
}

static void setupCamera(Scene& scene, int width, int height, Vec3 from, Vec3 to) {
  scene.width  = width;
  scene.height = height;
  float aspect = (float)width / (float)height;
  Matrix P = Matrix::perspective(0.001f, 1000.0f, aspect, 45.0f);
  Matrix V = Matrix::lookAt(from, to, Vec3(0.0f, 1.0f, 0.0f)).invert();
  scene.camera.init(P, width, height);
  scene.camera.updateView(V);
}

// -----------------------------------------------------------------------
// runTiled — the parallel pixel loop
// -----------------------------------------------------------------------

TEST_CASE("ReSTIR runTiled: visits every pixel exactly once (non-tile-multiple size)") {
  // 70x50 is deliberately NOT a multiple of TILE_SIZE — exercises the
  // boundary clamping in runPixelFunc.
  const int W = 70, H = 50;
  Scene scene;
  scene.width  = W;
  scene.height = H;
  Film film;
  film.init(W, H);
  ReSTIRInstantRadiosityIntegrator integrator(&scene, &film);

  std::vector<std::atomic<int>> visits(W * H);
  for (auto& v : visits) v.store(0);

  integrator.runTiled([&visits, W](int x, int y) {
    visits[y * W + x].fetch_add(1);
  });

  for (int i = 0; i < W * H; i++) {
    REQUIRE(visits[i].load() == 1);
  }
}

// -----------------------------------------------------------------------
// tracePrimary — the four exits of the specular walk
// -----------------------------------------------------------------------

TEST_CASE("ReSTIR tracePrimary: diffuse hit fills a gather record") {
  const int W = 16, H = 16;
  Texture white; fillTexture(white, 1, 1, Colour(1.0f, 1.0f, 1.0f));
  DiffuseBSDF floorMaterial(&white);
  Triangle floorTri = makeTri(Vec3(-10.0f, -10.0f, 0.0f), Vec3(10.0f, -10.0f, 0.0f),
                              Vec3(0.0f, 10.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f), 0);
  BackgroundColour background(Colour(0.0f, 0.0f, 0.0f));
  Scene scene;
  scene.init({floorTri}, {&floorMaterial}, &background);
  scene.build();
  setupCamera(scene, W, H, Vec3(0.0f, 0.0f, 3.0f), Vec3(0.0f, 0.0f, 0.0f));

  Film film;
  film.init(W, H);
  ReSTIRInstantRadiosityIntegrator integrator(&scene, &film);
  integrator.gBuffer.resize(W * H);   // render() does this; direct calls must too

  MTRandom sampler(42);
  int x = W / 2, y = H / 2;
  integrator.tracePrimary(x, y, &sampler);

  const PrimaryHit& hit = integrator.gBuffer[y * W + x];
  REQUIRE(hit.needsGather);
  REQUIRE(std::abs(hit.shadingData.x.z) < 1e-3f);          // on the floor plane
  REQUIRE(hit.shadingData.sNormal.z == Catch::Approx(1.0f).margin(1e-3f));
  REQUIRE(hit.throughput.r == Catch::Approx(1.0f));        // no specular chain crossed
  REQUIRE(hit.throughput.g == Catch::Approx(1.0f));
  REQUIRE(hit.throughput.b == Catch::Approx(1.0f));
}

TEST_CASE("ReSTIR tracePrimary: miss resolves to the background colour") {
  const int W = 16, H = 16;
  const Colour backgroundColour(0.2f, 0.4f, 0.6f);
  Texture white; fillTexture(white, 1, 1, Colour(1.0f, 1.0f, 1.0f));
  DiffuseBSDF floorMaterial(&white);
  Triangle floorTri = makeTri(Vec3(-10.0f, -10.0f, 0.0f), Vec3(10.0f, -10.0f, 0.0f),
                              Vec3(0.0f, 10.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f), 0);
  BackgroundColour background(backgroundColour);
  Scene scene;
  scene.init({floorTri}, {&floorMaterial}, &background);
  scene.build();
  // Camera looking AWAY from the floor (+z): every ray misses.
  setupCamera(scene, W, H, Vec3(0.0f, 0.0f, 3.0f), Vec3(0.0f, 0.0f, 6.0f));

  Film film;
  film.init(W, H);
  ReSTIRInstantRadiosityIntegrator integrator(&scene, &film);
  integrator.gBuffer.resize(W * H);

  MTRandom sampler(42);
  int x = W / 2, y = H / 2;
  integrator.tracePrimary(x, y, &sampler);

  const PrimaryHit& hit = integrator.gBuffer[y * W + x];
  REQUIRE_FALSE(hit.needsGather);
  REQUIRE(hit.resolved.r == Catch::Approx(backgroundColour.r).margin(1e-5f));
  REQUIRE(hit.resolved.g == Catch::Approx(backgroundColour.g).margin(1e-5f));
  REQUIRE(hit.resolved.b == Catch::Approx(backgroundColour.b).margin(1e-5f));
}

TEST_CASE("ReSTIR tracePrimary: emissive hit resolves to the emission") {
  const int W = 16, H = 16;
  const Colour emission(3.0f, 2.0f, 1.0f);
  Texture black; fillTexture(black, 1, 1, Colour(0.0f, 0.0f, 0.0f));
  DiffuseBSDF lightMaterial(&black);
  lightMaterial.addLight(emission);
  Triangle lightTri = makeTri(Vec3(-10.0f, -10.0f, 0.0f), Vec3(10.0f, -10.0f, 0.0f),
                              Vec3(0.0f, 10.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f), 0);
  BackgroundColour background(Colour(0.0f, 0.0f, 0.0f));
  Scene scene;
  scene.init({lightTri}, {&lightMaterial}, &background);
  scene.build();
  setupCamera(scene, W, H, Vec3(0.0f, 0.0f, 3.0f), Vec3(0.0f, 0.0f, 0.0f));

  Film film;
  film.init(W, H);
  ReSTIRInstantRadiosityIntegrator integrator(&scene, &film);
  integrator.gBuffer.resize(W * H);

  MTRandom sampler(42);
  int x = W / 2, y = H / 2;
  integrator.tracePrimary(x, y, &sampler);

  const PrimaryHit& hit = integrator.gBuffer[y * W + x];
  REQUIRE_FALSE(hit.needsGather);
  REQUIRE(hit.resolved.r == Catch::Approx(emission.r).margin(1e-5f));
  REQUIRE(hit.resolved.g == Catch::Approx(emission.g).margin(1e-5f));
  REQUIRE(hit.resolved.b == Catch::Approx(emission.b).margin(1e-5f));
}

TEST_CASE("ReSTIR tracePrimary: mirror corridor terminates black with needsGather false") {
  const int W = 16, H = 16;
  Texture white; fillTexture(white, 1, 1, Colour(1.0f, 1.0f, 1.0f));
  const Colour silverEta(0.177f, 0.178f, 0.172f);
  const Colour silverK(3.638f, 2.973f, 2.430f);
  MirrorBSDF bottomMirror(&white, silverEta, silverK);
  MirrorBSDF topMirror(&white, silverEta, silverK);

  Triangle bottomTri = makeTri(Vec3(-10.0f, -10.0f, 0.0f), Vec3(10.0f, -10.0f, 0.0f),
                               Vec3(0.0f, 10.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f), 0);
  Triangle topTri    = makeTri(Vec3(-10.0f, -10.0f, 2.0f), Vec3(10.0f, -10.0f, 2.0f),
                               Vec3(0.0f, 10.0f, 2.0f), Vec3(0.0f, 0.0f, -1.0f), 1);
  BackgroundColour background(Colour(0.9f, 0.9f, 0.9f)); // leakage would show up
  Scene scene;
  scene.init({bottomTri, topTri}, {&bottomMirror, &topMirror}, &background);
  scene.build();
  setupCamera(scene, W, H, Vec3(0.1f, 0.1f, 1.0f), Vec3(0.1f, 0.1f, 0.0f));

  Film film;
  film.init(W, H);
  ReSTIRInstantRadiosityIntegrator integrator(&scene, &film);
  integrator.gBuffer.resize(W * H);

  MTRandom sampler(42);
  int x = W / 2, y = H / 2;
  integrator.tracePrimary(x, y, &sampler);

  const PrimaryHit& hit = integrator.gBuffer[y * W + x];
  REQUIRE_FALSE(hit.needsGather);
  REQUIRE(std::isfinite(hit.resolved.r));
  REQUIRE(hit.resolved.r == Catch::Approx(0.0f).margin(1e-6f));
}

TEST_CASE("ReSTIR tracePrimary: fills film normals and albedo at SPP zero") {
  const int W = 16, H = 16;
  Texture white; fillTexture(white, 1, 1, Colour(1.0f, 1.0f, 1.0f));
  DiffuseBSDF floorMaterial(&white);
  Triangle floorTri = makeTri(Vec3(-10.0f, -10.0f, 0.0f), Vec3(10.0f, -10.0f, 0.0f),
                              Vec3(0.0f, 10.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f), 0);
  BackgroundColour background(Colour(0.0f, 0.0f, 0.0f));
  Scene scene;
  scene.init({floorTri}, {&floorMaterial}, &background);
  scene.build();
  setupCamera(scene, W, H, Vec3(0.0f, 0.0f, 3.0f), Vec3(0.0f, 0.0f, 0.0f));

  Film film;
  film.init(W, H);
  REQUIRE(film.SPP == 0);
  ReSTIRInstantRadiosityIntegrator integrator(&scene, &film);
  integrator.gBuffer.resize(W * H);

  MTRandom sampler(42);
  int x = W / 2, y = H / 2;
  integrator.tracePrimary(x, y, &sampler);

  // Normal (0,0,1) stored as Colour(0,0,1); albedo buffer holds evaluate() = albedo/PI.
  const Colour& storedNormal = film.filmNormals[y * W + x];
  REQUIRE(storedNormal.b == Catch::Approx(1.0f).margin(1e-3f));
  const Colour& storedAlbedo = film.filmAlbedos[y * W + x];
  REQUIRE(storedAlbedo.r == Catch::Approx(1.0f / PI).margin(1e-3f));
}

// -----------------------------------------------------------------------
// pHat — the scalar target function
// -----------------------------------------------------------------------

// Floor scene + shading point at the origin, VPL 2 units above facing down:
// unshadowed contribution = (albedo/PI) * G = 0.25/PI per channel (white
// albedo, radiance 1). Luminance weights sum to 1, so pHat = 0.25/PI.
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

TEST_CASE("ReSTIR pHat: analytical value and consistency with unshadowedVPLContribution") {
  Texture white; fillTexture(white, 1, 1, Colour(1.0f, 1.0f, 1.0f));
  DiffuseBSDF floorMaterial(&white);
  BackgroundColour background(Colour(0.0f, 0.0f, 0.0f));
  Scene scene = makeFloorScene(&floorMaterial, &background);

  Film film;
  film.init(8, 8);
  ReSTIRInstantRadiosityIntegrator integrator(&scene, &film);

  VPL vpl;
  vpl.position = Vec3(0.0f, 0.0f, 2.0f);
  vpl.normal   = Vec3(0.0f, 0.0f, -1.0f);
  vpl.radiance = Colour(1.0f, 1.0f, 1.0f);

  ShadingData shadingData = shadeFloorOrigin(scene);
  float target = integrator.pHat(shadingData, vpl);

  REQUIRE(target == Catch::Approx(0.25f / PI).epsilon(1e-3f));
  REQUIRE(target == Catch::Approx(integrator.unshadowedVPLContribution(shadingData, vpl, vpl.position).lum()));
}

TEST_CASE("ReSTIR pHat: ignores visibility — occluded VPL still scores (the hoist contract)") {
  // pHat is the EYEBALL score: it must not trace rays, so an occluder between
  // the shading point and the VPL must not change it — while the baseline
  // gather (which applies visibility) returns zero for the same setup.
  Texture white; fillTexture(white, 1, 1, Colour(1.0f, 1.0f, 1.0f));
  DiffuseBSDF floorMaterial(&white);
  DiffuseBSDF occluderMaterial(&white);
  BackgroundColour background(Colour(0.0f, 0.0f, 0.0f));

  Triangle floorTri    = makeTri(Vec3(-10.0f, -10.0f, 0.0f), Vec3(10.0f, -10.0f, 0.0f),
                                 Vec3(0.0f, 10.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f), 0);
  Triangle occluderTri = makeTri(Vec3(-0.5f, -0.5f, 1.0f), Vec3(0.5f, -0.5f, 1.0f),
                                 Vec3(0.0f, 0.5f, 1.0f), Vec3(0.0f, 0.0f, 1.0f), 1);
  Scene scene;
  scene.init({floorTri, occluderTri}, {&floorMaterial, &occluderMaterial}, &background);
  scene.build();

  Film film;
  film.init(8, 8);
  ReSTIRInstantRadiosityIntegrator integrator(&scene, &film);

  VPL vpl;
  vpl.position = Vec3(0.0f, 0.0f, 2.0f);
  vpl.normal   = Vec3(0.0f, 0.0f, -1.0f);
  vpl.radiance = Colour(1.0f, 1.0f, 1.0f);
  integrator.vpls.push_back(vpl);

  // Reach the floor origin from an angle that misses the small occluder.
  Ray ray(Vec3(3.0f, 3.0f, 3.0f), Vec3(-1.0f, -1.0f, -1.0f).normalize());
  IntersectionData intersection = scene.traverse(ray);
  REQUIRE(intersection.t < FLT_MAX);
  ShadingData shadingData = scene.calculateShadingData(intersection, ray);

  float target = integrator.pHat(shadingData, vpl);
  REQUIRE(target == Catch::Approx(0.25f / PI).epsilon(1e-3f));   // sees THROUGH the occluder

  MTRandom gatherSampler(42);
  Colour gathered = integrator.gatherVPLs(shadingData, &gatherSampler);
  REQUIRE(gathered.r == Catch::Approx(0.0f).margin(1e-6f));      // gather does not
}

TEST_CASE("ReSTIR pHat: zero for geometrically dark configurations") {
  Texture white; fillTexture(white, 1, 1, Colour(1.0f, 1.0f, 1.0f));
  DiffuseBSDF floorMaterial(&white);
  BackgroundColour background(Colour(0.0f, 0.0f, 0.0f));
  Scene scene = makeFloorScene(&floorMaterial, &background);

  Film film;
  film.init(8, 8);
  ReSTIRInstantRadiosityIntegrator integrator(&scene, &film);
  ShadingData shadingData = shadeFloorOrigin(scene);

  VPL behind;                                   // below the floor: cos_x = 0
  behind.position = Vec3(0.0f, 0.0f, -2.0f);
  behind.normal   = Vec3(0.0f, 0.0f, 1.0f);
  behind.radiance = Colour(1.0f, 1.0f, 1.0f);
  REQUIRE(integrator.pHat(shadingData, behind) == 0.0f);

  VPL facingAway;                               // glowing side points away: cos_vpl = 0
  facingAway.position = Vec3(0.0f, 0.0f, 2.0f);
  facingAway.normal   = Vec3(0.0f, 0.0f, 1.0f);
  facingAway.radiance = Colour(1.0f, 1.0f, 1.0f);
  REQUIRE(integrator.pHat(shadingData, facingAway) == 0.0f);
}

// -----------------------------------------------------------------------
// generateCandidates — phase 1b
// -----------------------------------------------------------------------

// Builds an integrator over the floor scene with a hand-set gather record at
// pixel 0, sized buffers, and the given VPL pool. Camera not needed:
// generateCandidates reads only gBuffer[pixel].shadingData and vpls.
static void setupCandidateFixture(ReSTIRInstantRadiosityIntegrator& integrator,
                                  Scene& scene, std::vector<VPL> pool) {
  integrator.gBuffer.resize(4);
  integrator.reservoirs.resize(4);
  integrator.reservoirsPrev.resize(4);
  integrator.gBuffer[0].shadingData = shadeFloorOrigin(scene);
  integrator.gBuffer[0].needsGather = true;
  integrator.vpls = pool;
}

static VPL makeOverheadVPL(float height, Colour radiance = Colour(1.0f, 1.0f, 1.0f)) {
  VPL vpl;
  vpl.position = Vec3(0.0f, 0.0f, height);
  vpl.normal   = Vec3(0.0f, 0.0f, -1.0f);
  vpl.radiance = radiance;
  return vpl;
}

TEST_CASE("ReSTIR generateCandidates: empty pool leaves a clean empty reservoir") {
  Texture white; fillTexture(white, 1, 1, Colour(1.0f, 1.0f, 1.0f));
  DiffuseBSDF floorMaterial(&white);
  BackgroundColour background(Colour(0.0f, 0.0f, 0.0f));
  Scene scene = makeFloorScene(&floorMaterial, &background);
  Film film; film.init(8, 8);
  ReSTIRInstantRadiosityIntegrator integrator(&scene, &film);
  setupCandidateFixture(integrator, scene, {});   // no VPLs (classroom starvation case)

  MTRandom sampler(42);
  integrator.generateCandidates(0, &sampler);

  REQUIRE(integrator.reservoirs[0].vplIndex == -1);
  REQUIRE(integrator.reservoirs[0].contributionWeight == 0.0f);
}

TEST_CASE("ReSTIR generateCandidates: non-gather pixel resets a previously populated reservoir") {
  // The stale-state regression: reservoirs[] holds LAST pass's reservoir.
  // A pixel that resolved terminally this pass must overwrite it with a clean
  // empty one — otherwise a dead VPL index from an old constellation leaks
  // into shading.
  Texture white; fillTexture(white, 1, 1, Colour(1.0f, 1.0f, 1.0f));
  DiffuseBSDF floorMaterial(&white);
  BackgroundColour background(Colour(0.0f, 0.0f, 0.0f));
  Scene scene = makeFloorScene(&floorMaterial, &background);
  Film film; film.init(8, 8);
  ReSTIRInstantRadiosityIntegrator integrator(&scene, &film);
  setupCandidateFixture(integrator, scene, {makeOverheadVPL(2.0f)});

  integrator.gBuffer[0].needsGather = false;      // terminal pixel this pass
  integrator.reservoirs[0].vplIndex = 7;          // junk from "last pass"
  integrator.reservoirs[0].weightSum = 5.0f;
  integrator.reservoirs[0].candidateCount = 32.0f;
  integrator.reservoirs[0].contributionWeight = 3.0f;

  MTRandom sampler(42);
  integrator.generateCandidates(0, &sampler);

  REQUIRE(integrator.reservoirs[0].vplIndex == -1);
  REQUIRE(integrator.reservoirs[0].weightSum == 0.0f);
  REQUIRE(integrator.reservoirs[0].contributionWeight == 0.0f);
}

TEST_CASE("ReSTIR generateCandidates: single-VPL pool gives W = 1 exactly") {
  // Pool of one: every candidate is that VPL, weight = pHat * 1, so
  // W = (M * pHat) / (M * pHat) = 1 — independent of M and of the randoms.
  Texture white; fillTexture(white, 1, 1, Colour(1.0f, 1.0f, 1.0f));
  DiffuseBSDF floorMaterial(&white);
  BackgroundColour background(Colour(0.0f, 0.0f, 0.0f));
  Scene scene = makeFloorScene(&floorMaterial, &background);
  Film film; film.init(8, 8);
  ReSTIRInstantRadiosityIntegrator integrator(&scene, &film);
  setupCandidateFixture(integrator, scene, {makeOverheadVPL(2.0f)});

  MTRandom sampler(42);
  integrator.generateCandidates(0, &sampler);

  REQUIRE(integrator.reservoirs[0].vplIndex == 0);
  REQUIRE(integrator.reservoirs[0].contributionWeight == Catch::Approx(1.0f).epsilon(1e-4f));
}

TEST_CASE("ReSTIR generateCandidates: identical VPLs give W = N (uniform anchor)") {
  // N copies of the same VPL: all pHats equal, so W = N exactly — ReSTIR
  // degrading to 'pick one of N, multiply by N'. Catches a missing
  // '* poolSize' in the candidate weight (which would give W = 1 instead).
  Texture white; fillTexture(white, 1, 1, Colour(1.0f, 1.0f, 1.0f));
  DiffuseBSDF floorMaterial(&white);
  BackgroundColour background(Colour(0.0f, 0.0f, 0.0f));
  Scene scene = makeFloorScene(&floorMaterial, &background);
  Film film; film.init(8, 8);
  ReSTIRInstantRadiosityIntegrator integrator(&scene, &film);

  const int N = 17;
  std::vector<VPL> pool;
  for (int i = 0; i < N; i++) pool.push_back(makeOverheadVPL(2.0f));
  setupCandidateFixture(integrator, scene, pool);

  MTRandom sampler(42);
  integrator.generateCandidates(0, &sampler);

  REQUIRE(integrator.reservoirs[0].vplIndex >= 0);
  REQUIRE(integrator.reservoirs[0].contributionWeight == Catch::Approx((float)N).epsilon(1e-3f));
}

TEST_CASE("ReSTIR generateCandidates: winner frequency follows pHat (near vs far VPL)") {
  // VPL A at height 2 (G = 0.25) vs VPL B at height 4 (G = 0.0625): pHat
  // ratio 4:1, so A should win ~80% of auditions. Statistical over seeds.
  Texture white; fillTexture(white, 1, 1, Colour(1.0f, 1.0f, 1.0f));
  DiffuseBSDF floorMaterial(&white);
  BackgroundColour background(Colour(0.0f, 0.0f, 0.0f));
  Scene scene = makeFloorScene(&floorMaterial, &background);
  Film film; film.init(8, 8);
  ReSTIRInstantRadiosityIntegrator integrator(&scene, &film);
  setupCandidateFixture(integrator, scene, {makeOverheadVPL(2.0f), makeOverheadVPL(4.0f)});

  const int trials = 3000;
  int nearWins = 0;
  for (int t = 0; t < trials; t++) {
    MTRandom sampler(t + 1);
    integrator.generateCandidates(0, &sampler);
    REQUIRE(integrator.reservoirs[0].vplIndex >= 0);
    if (integrator.reservoirs[0].vplIndex == 0) nearWins++;
  }

  REQUIRE((float)nearWins / trials == Catch::Approx(0.8f).margin(0.03f));
}

// -----------------------------------------------------------------------
// selectRandomNeighbour
// -----------------------------------------------------------------------

TEST_CASE("ReSTIR selectRandomNeighbour: in bounds, within radius, never self") {
  const int W = 64, H = 64;
  Scene scene;
  scene.width  = W;
  scene.height = H;
  Film film; film.init(W, H);
  ReSTIRInstantRadiosityIntegrator integrator(&scene, &film);

  MTRandom sampler(42);
  const int x = 32, y = 32;   // interior pixel: radius disk fully inside
  for (int i = 0; i < 2000; i++) {
    int neighbourIndex = integrator.selectRandomNeighbour(x, y, &sampler);
    if (neighbourIndex < 0) continue;   // only the self-rejection can fire here
    int nx = neighbourIndex % W;
    int ny = neighbourIndex / W;
    REQUIRE(nx >= 0); REQUIRE(nx < W);
    REQUIRE(ny >= 0); REQUIRE(ny < H);
    REQUIRE_FALSE((nx == x && ny == y));
    float dx = (float)(nx - x), dy = (float)(ny - y);
    REQUIRE(std::sqrt(dx * dx + dy * dy) <= Config::IR_RESTIR_RADIUS + 1.0f); // +1: rounding
  }
}

TEST_CASE("ReSTIR selectRandomNeighbour: corner pixel never returns out-of-bounds") {
  const int W = 16, H = 16;
  Scene scene;
  scene.width  = W;
  scene.height = H;
  Film film; film.init(W, H);
  ReSTIRInstantRadiosityIntegrator integrator(&scene, &film);

  MTRandom sampler(7);
  for (int i = 0; i < 2000; i++) {
    int neighbourIndex = integrator.selectRandomNeighbour(0, 0, &sampler);
    REQUIRE(neighbourIndex < W * H);   // -1 (rejected) or a valid index
    if (neighbourIndex >= 0) {
      REQUIRE(neighbourIndex != 0);    // never self
    }
  }
}

// -----------------------------------------------------------------------
// spatialReuse
// -----------------------------------------------------------------------

// 2x2 image; "me" is pixel (0,0). FixedSampler {0.0, 0.01, 0.5} aims every
// neighbour pick at (1,0): angle 0, radius sqrt(0.01)*RADIUS = 0.1*RADIUS
// (rounds to 1 for the default radius 10), merge random 0.5.
//
// Common setup: gather records for pixels 0 and 1, both on the floor;
// my previous reservoir holds a WEAK winner (VPL 0), the neighbour holds a
// STRONG one (VPL 1: big W and M). Returns my pixel's ShadingData.
static ShadingData setupReuseBuffers(ReSTIRInstantRadiosityIntegrator& integrator, Scene& scene) {
  integrator.gBuffer.resize(4);
  integrator.reservoirs.resize(4);
  integrator.reservoirsPrev.resize(4);
  integrator.vpls = {makeOverheadVPL(2.0f), makeOverheadVPL(4.0f)};

  ShadingData shadingData = shadeFloorOrigin(scene);
  integrator.gBuffer[0].shadingData = shadingData;
  integrator.gBuffer[0].needsGather = true;
  integrator.gBuffer[1].shadingData = shadingData;   // same surface -> gates pass
  integrator.gBuffer[1].needsGather = true;

  Reservoir mine;                    // weak: tiny weight mass behind VPL 0
  mine.vplIndex = 0;
  mine.weightSum = 0.001f;
  mine.candidateCount = 1.0f;
  integrator.reservoirsPrev[0] = mine;

  Reservoir theirs;                  // strong: VPL 1 with big W * M
  theirs.vplIndex = 1;
  theirs.weightSum = 100.0f;
  theirs.candidateCount = 32.0f;
  theirs.contributionWeight = 5.0f;
  integrator.reservoirsPrev[1] = theirs;

  return shadingData;
}

TEST_CASE("ReSTIR spatialReuse: adopts a dominant neighbour winner and re-finalizes W") {
  Texture white; fillTexture(white, 1, 1, Colour(1.0f, 1.0f, 1.0f));
  DiffuseBSDF floorMaterial(&white);
  BackgroundColour background(Colour(0.0f, 0.0f, 0.0f));
  Scene scene = makeFloorScene(&floorMaterial, &background);
  scene.width  = 2;
  scene.height = 2;
  Film film; film.init(2, 2);
  ReSTIRInstantRadiosityIntegrator integrator(&scene, &film);
  ShadingData shadingData = setupReuseBuffers(integrator, scene);

  FixedSampler sampler{0.0f, 0.01f, 0.5f};   // always aims at pixel (1,0)
  integrator.spatialReuse(0, 0, 0, &sampler);

  const Reservoir& result = integrator.reservoirs[0];
  // Merge weight = pHat_mine(VPL 1) * 5 * 32 >> my 0.001 -> adoption certain.
  REQUIRE(result.vplIndex == 1);

  // Head-count absorbed once per merge iteration (same neighbour K times).
  REQUIRE(result.candidateCount ==
          Catch::Approx(1.0f + (float)Config::IR_RESTIR_K * 32.0f));

  // Re-finalize happened: W matches the formula computed from the RESULT
  // fields and MY pHat of the new winner (a stale W from phase 1 would not).
  float winnerPHat = integrator.pHat(shadingData, integrator.vpls[1]);
  REQUIRE(result.contributionWeight ==
          Catch::Approx(result.weightSum / (result.candidateCount * winnerPHat)).epsilon(1e-3f));
}

TEST_CASE("ReSTIR spatialReuse: normal and depth gates reject dissimilar neighbours") {
  Texture white; fillTexture(white, 1, 1, Colour(1.0f, 1.0f, 1.0f));
  DiffuseBSDF floorMaterial(&white);
  BackgroundColour background(Colour(0.0f, 0.0f, 0.0f));

  SECTION("wall-like neighbour (perpendicular normal)") {
    Scene scene = makeFloorScene(&floorMaterial, &background);
    scene.width = 2; scene.height = 2;
    Film film; film.init(2, 2);
    ReSTIRInstantRadiosityIntegrator integrator(&scene, &film);
    setupReuseBuffers(integrator, scene);
    integrator.gBuffer[1].shadingData.sNormal = Vec3(1.0f, 0.0f, 0.0f);  // dot = 0

    FixedSampler sampler{0.0f, 0.01f, 0.5f};
    integrator.spatialReuse(0, 0, 0, &sampler);

    REQUIRE(integrator.reservoirs[0].vplIndex == 0);                      // kept my winner
    REQUIRE(integrator.reservoirs[0].candidateCount == Catch::Approx(1.0f)); // no dilution
  }

  SECTION("far-away neighbour (depth gap)") {
    Scene scene = makeFloorScene(&floorMaterial, &background);
    scene.width = 2; scene.height = 2;
    Film film; film.init(2, 2);
    ReSTIRInstantRadiosityIntegrator integrator(&scene, &film);
    setupReuseBuffers(integrator, scene);
    integrator.gBuffer[1].shadingData.t = 100.0f;   // mine is ~3: relative gap huge

    FixedSampler sampler{0.0f, 0.01f, 0.5f};
    integrator.spatialReuse(0, 0, 0, &sampler);

    REQUIRE(integrator.reservoirs[0].vplIndex == 0);
    REQUIRE(integrator.reservoirs[0].candidateCount == Catch::Approx(1.0f));
  }
}

TEST_CASE("ReSTIR spatialReuse: empty neighbour reservoir is skipped, no dilution") {
  Texture white; fillTexture(white, 1, 1, Colour(1.0f, 1.0f, 1.0f));
  DiffuseBSDF floorMaterial(&white);
  BackgroundColour background(Colour(0.0f, 0.0f, 0.0f));
  Scene scene = makeFloorScene(&floorMaterial, &background);
  scene.width = 2; scene.height = 2;
  Film film; film.init(2, 2);
  ReSTIRInstantRadiosityIntegrator integrator(&scene, &film);
  setupReuseBuffers(integrator, scene);
  integrator.reservoirsPrev[1] = Reservoir{};     // neighbour found nothing

  FixedSampler sampler{0.0f, 0.01f, 0.5f};
  integrator.spatialReuse(0, 0, 0, &sampler);

  REQUIRE(integrator.reservoirs[0].vplIndex == 0);
  REQUIRE(integrator.reservoirs[0].candidateCount == Catch::Approx(1.0f));
}

TEST_CASE("ReSTIR spatialReuse: terminal pixel copies its previous reservoir through") {
  Texture white; fillTexture(white, 1, 1, Colour(1.0f, 1.0f, 1.0f));
  DiffuseBSDF floorMaterial(&white);
  BackgroundColour background(Colour(0.0f, 0.0f, 0.0f));
  Scene scene = makeFloorScene(&floorMaterial, &background);
  scene.width = 2; scene.height = 2;
  Film film; film.init(2, 2);
  ReSTIRInstantRadiosityIntegrator integrator(&scene, &film);
  setupReuseBuffers(integrator, scene);
  integrator.gBuffer[0].needsGather = false;      // terminal this pass
  integrator.reservoirs[0].vplIndex = 9;          // stale garbage in the write buffer

  FixedSampler sampler{0.0f, 0.01f, 0.5f};
  integrator.spatialReuse(0, 0, 0, &sampler);

  REQUIRE(integrator.reservoirs[0].vplIndex == 0);   // Prev copied through, garbage gone
  REQUIRE(integrator.reservoirs[0].weightSum == Catch::Approx(0.001f));
}

// -----------------------------------------------------------------------
// shadePixel
// -----------------------------------------------------------------------

// Floor + a small off-axis area light (so computeDirectMIS has a light to
// sample — the scene must never have an empty lights list).
static Scene makeLitFloorScene(DiffuseBSDF* floorMaterial, DiffuseBSDF* lightMaterial,
                               BackgroundColour* background) {
  Triangle lightTri = makeTri(Vec3(2.5f, -0.5f, 3.0f), Vec3(3.5f, -0.5f, 3.0f),
                              Vec3(2.5f, 0.5f, 3.0f), Vec3(0.0f, 0.0f, -1.0f), 0);
  Triangle floorTri = makeTri(Vec3(-10.0f, -10.0f, 0.0f), Vec3(10.0f, -10.0f, 0.0f),
                              Vec3(0.0f, 10.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f), 1);
  Scene scene;
  scene.init({lightTri, floorTri}, {lightMaterial, floorMaterial}, background);
  scene.build();
  scene.width  = 2;
  scene.height = 2;
  return scene;
}

TEST_CASE("ReSTIR shadePixel: terminal pixel returns its resolved colour") {
  Texture white; fillTexture(white, 1, 1, Colour(1.0f, 1.0f, 1.0f));
  DiffuseBSDF floorMaterial(&white);
  BackgroundColour background(Colour(0.0f, 0.0f, 0.0f));
  Scene scene = makeFloorScene(&floorMaterial, &background);
  Film film; film.init(2, 2);
  ReSTIRInstantRadiosityIntegrator integrator(&scene, &film);
  integrator.gBuffer.resize(4);
  integrator.reservoirs.resize(4);
  integrator.gBuffer[0].needsGather = false;
  integrator.gBuffer[0].resolved = Colour(1.0f, 2.0f, 3.0f);

  MTRandom sampler(42);
  Colour result = integrator.shadePixel(0, &sampler);
  REQUIRE(result.r == Catch::Approx(1.0f));
  REQUIRE(result.g == Catch::Approx(2.0f));
  REQUIRE(result.b == Catch::Approx(3.0f));
}

TEST_CASE("ReSTIR shadePixel: winner adds exactly contribution * W on top of direct") {
  // Identical FixedSamplers make the direct term cancel in the difference
  // (the indirect path consumes no randomness), isolating the RIS estimate.
  const Colour emission(5.0f, 5.0f, 5.0f);
  Texture black; fillTexture(black, 1, 1, Colour(0.0f, 0.0f, 0.0f));
  Texture white; fillTexture(white, 1, 1, Colour(1.0f, 1.0f, 1.0f));
  DiffuseBSDF lightMaterial(&black);
  lightMaterial.addLight(emission);
  DiffuseBSDF floorMaterial(&white);
  BackgroundColour background(Colour(0.0f, 0.0f, 0.0f));
  Scene scene = makeLitFloorScene(&floorMaterial, &lightMaterial, &background);
  Film film; film.init(2, 2);
  ReSTIRInstantRadiosityIntegrator integrator(&scene, &film);

  integrator.gBuffer.resize(4);
  integrator.reservoirs.resize(4);
  integrator.vpls = {makeOverheadVPL(2.0f, Colour(2.0f, 0.5f, 0.25f))};
  ShadingData shadingData = shadeFloorOrigin(scene);
  integrator.gBuffer[0].shadingData = shadingData;
  integrator.gBuffer[0].throughput = Colour(0.5f, 0.5f, 0.5f);   // pretend specular chain tint
  integrator.gBuffer[0].needsGather = true;

  FixedSampler samplerA{0.1f, 0.4f, 0.4f};
  Colour withoutWinner = integrator.shadePixel(0, &samplerA);    // reservoir empty

  integrator.reservoirs[0].vplIndex = 0;
  integrator.reservoirs[0].contributionWeight = 2.5f;
  FixedSampler samplerB{0.1f, 0.4f, 0.4f};
  Colour withWinner = integrator.shadePixel(0, &samplerB);

  Colour expected = integrator.unshadowedVPLContribution(shadingData, integrator.vpls[0],
                                                         integrator.vpls[0].position)
                    * 2.5f * 0.5f;                                // * W * throughput
  Colour difference = withWinner - withoutWinner;
  REQUIRE(difference.r == Catch::Approx(expected.r).epsilon(1e-3f));
  REQUIRE(difference.g == Catch::Approx(expected.g).epsilon(1e-3f));
  REQUIRE(difference.b == Catch::Approx(expected.b).epsilon(1e-3f));
}

TEST_CASE("ReSTIR shadePixel: occluded winner contributes zero indirect") {
  const Colour emission(5.0f, 5.0f, 5.0f);
  Texture black; fillTexture(black, 1, 1, Colour(0.0f, 0.0f, 0.0f));
  Texture white; fillTexture(white, 1, 1, Colour(1.0f, 1.0f, 1.0f));
  DiffuseBSDF lightMaterial(&black);
  lightMaterial.addLight(emission);
  DiffuseBSDF floorMaterial(&white);
  DiffuseBSDF occluderMaterial(&white);
  BackgroundColour background(Colour(0.0f, 0.0f, 0.0f));

  Triangle lightTri    = makeTri(Vec3(2.5f, -0.5f, 3.0f), Vec3(3.5f, -0.5f, 3.0f),
                                 Vec3(2.5f, 0.5f, 3.0f), Vec3(0.0f, 0.0f, -1.0f), 0);
  Triangle floorTri    = makeTri(Vec3(-10.0f, -10.0f, 0.0f), Vec3(10.0f, -10.0f, 0.0f),
                                 Vec3(0.0f, 10.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f), 1);
  Triangle occluderTri = makeTri(Vec3(-0.5f, -0.5f, 1.0f), Vec3(0.5f, -0.5f, 1.0f),
                                 Vec3(0.0f, 0.5f, 1.0f), Vec3(0.0f, 0.0f, 1.0f), 2);
  Scene scene;
  scene.init({lightTri, floorTri, occluderTri},
             {&lightMaterial, &floorMaterial, &occluderMaterial}, &background);
  scene.build();
  scene.width = 2; scene.height = 2;
  Film film; film.init(2, 2);
  ReSTIRInstantRadiosityIntegrator integrator(&scene, &film);

  integrator.gBuffer.resize(4);
  integrator.reservoirs.resize(4);
  integrator.vpls = {makeOverheadVPL(2.0f)};   // directly above, behind the occluder

  // Shading point at the floor origin, reached at an angle missing the occluder.
  Ray ray(Vec3(3.0f, 3.0f, 3.0f), Vec3(-1.0f, -1.0f, -1.0f).normalize());
  IntersectionData intersection = scene.traverse(ray);
  REQUIRE(intersection.t < FLT_MAX);
  integrator.gBuffer[0].shadingData = scene.calculateShadingData(intersection, ray);
  integrator.gBuffer[0].throughput = Colour(1.0f, 1.0f, 1.0f);
  integrator.gBuffer[0].needsGather = true;

  FixedSampler samplerA{0.1f, 0.4f, 0.4f};
  Colour withoutWinner = integrator.shadePixel(0, &samplerA);

  integrator.reservoirs[0].vplIndex = 0;
  integrator.reservoirs[0].contributionWeight = 2.5f;
  FixedSampler samplerB{0.1f, 0.4f, 0.4f};
  Colour withWinner = integrator.shadePixel(0, &samplerB);

  // The one shadow ray finds the occluder: identical results.
  REQUIRE((withWinner - withoutWinner).lum() == Catch::Approx(0.0f).margin(1e-6f));
}

TEST_CASE("ReSTIR generateCandidates: deterministic for a fixed seed") {
  Texture white; fillTexture(white, 1, 1, Colour(1.0f, 1.0f, 1.0f));
  DiffuseBSDF floorMaterial(&white);
  BackgroundColour background(Colour(0.0f, 0.0f, 0.0f));
  Scene scene = makeFloorScene(&floorMaterial, &background);
  Film film; film.init(8, 8);
  ReSTIRInstantRadiosityIntegrator integrator(&scene, &film);
  setupCandidateFixture(integrator, scene,
                        {makeOverheadVPL(2.0f), makeOverheadVPL(3.0f), makeOverheadVPL(4.0f)});

  MTRandom samplerA(1234);
  integrator.generateCandidates(0, &samplerA);
  int winnerA = integrator.reservoirs[0].vplIndex;
  float weightA = integrator.reservoirs[0].contributionWeight;

  MTRandom samplerB(1234);
  integrator.generateCandidates(0, &samplerB);
  REQUIRE(integrator.reservoirs[0].vplIndex == winnerA);
  REQUIRE(integrator.reservoirs[0].contributionWeight == Catch::Approx(weightA));
}
