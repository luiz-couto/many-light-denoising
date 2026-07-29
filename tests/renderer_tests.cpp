#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "renderer.h"
#include "geometry.h"
#include "shading.h"
#include "texture.h"
#include <cmath>

// -----------------------------------------------------------------------
// Mock BSDF — emissive WHITE so that every ray hit at depth=0 returns WHITE.
// No sampler is consumed in pathTrace for depth-0 emissive hits, making
// pixel values analytically predictable regardless of subpixel jitter.
// -----------------------------------------------------------------------

class RendererEmissiveBSDF : public BSDF {
public:
  RendererEmissiveBSDF() { emission = Colour(1.0f, 1.0f, 1.0f); }
  Vec3   sample(const ShadingData&, Sampler*, Colour& w, float& pdf) override
  { w = Colour(1.0f, 1.0f, 1.0f); pdf = 1.0f; return Vec3(0.0f, 0.0f, 1.0f); }
  Colour evaluate(const ShadingData&, const Vec3&) override { return Colour(1.0f, 1.0f, 1.0f); }
  float  PDF(const ShadingData&, const Vec3&) override { return 1.0f; }
  bool   isPureSpecular() override { return false; }
  bool   isTwoSided() override { return true; }
  float  mask(const ShadingData&) override { return 1.0f; }
};

static constexpr int TEST_W = 800;
static constexpr int TEST_H = 600;

// Large +Z triangle at z=0 covering the entire camera frustum (vertices at ±10 vs frustum ±2.8).
// Camera at (0,0,5) looking at origin, FOV 45, aspect 800/600.
// Every camera ray — regardless of subpixel jitter — hits this triangle.
static void setupEmissiveScene(Renderer& r, RendererEmissiveBSDF& mat, BackgroundColour& bg) {
  Vertex v0(Vec3(-10.0f, -10.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f), 0.0f, 0.0f);
  Vertex v1(Vec3( 10.0f, -10.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f), 1.0f, 0.0f);
  Vertex v2(Vec3(  0.0f,  10.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f), 0.5f, 1.0f);
  Triangle tri; tri.init(v0, v1, v2, 0);

  r.scene.init({tri}, {&mat}, &bg);
  r.scene.build();
  r.scene.width  = TEST_W;
  r.scene.height = TEST_H;
  r.film.init(TEST_W, TEST_H);

  float aspect = (float)TEST_W / (float)TEST_H;
  Matrix P = Matrix::perspective(0.001f, 1000.0f, aspect, 45.0f);
  Matrix V = Matrix::lookAt(Vec3(0.0f, 0.0f, 5.0f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f)).invert();
  r.scene.camera.init(P, TEST_W, TEST_H);
  r.scene.camera.updateView(V);
}

// -----------------------------------------------------------------------
// Tests
// -----------------------------------------------------------------------

TEST_CASE("Renderer render: film is zero before first render call") {
  RendererEmissiveBSDF mat;
  BackgroundColour bg(Colour(0.0f, 0.0f, 0.0f));
  Renderer r;
  setupEmissiveScene(r, mat, bg);

  for (const Colour& c : r.film.film) {
    REQUIRE(c.r == Catch::Approx(0.0f));
    REQUIRE(c.g == Catch::Approx(0.0f));
    REQUIRE(c.b == Catch::Approx(0.0f));
  }
}

TEST_CASE("Renderer render: SPP is 1 after single render call") {
  RendererEmissiveBSDF mat;
  BackgroundColour bg(Colour(0.0f, 0.0f, 0.0f));
  Renderer r;
  setupEmissiveScene(r, mat, bg);

  r.render();

  REQUIRE(r.film.SPP == 1);
}

TEST_CASE("Renderer render: SPP increments with each progressive render call") {
  // render() does not clear the film, so each call adds one SPP.
  RendererEmissiveBSDF mat;
  BackgroundColour bg(Colour(0.0f, 0.0f, 0.0f));
  Renderer r;
  setupEmissiveScene(r, mat, bg);

  r.render();
  r.render();

  REQUIRE(r.film.SPP == 2);
}

TEST_CASE("Renderer render: emissive geometry illuminates center pixel") {
  RendererEmissiveBSDF mat;
  BackgroundColour bg(Colour(0.0f, 0.0f, 0.0f));
  Renderer r;
  setupEmissiveScene(r, mat, bg);

  r.render();

  int cx = r.scene.width  / 2;
  int cy = r.scene.height / 2;
  Colour px = r.film.film[cy * r.film.width + cx];
  REQUIRE(px.r > 0.0f);
  REQUIRE(px.g > 0.0f);
  REQUIRE(px.b > 0.0f);
}

TEST_CASE("Renderer render: full-screen emissive produces near-unity average brightness") {
  // Every tile is processed and every pixel receives a WHITE sample.
  // If sampler jitter rarely truncates to the neighbour pixel, one pixel may lose its
  // contribution while its neighbour gains a duplicate — the per-pixel sum is conserved.
  // Testing the average brightness (sum / pixel count) is therefore robust and also
  // verifies energy conservation: average must be ≈ 1.0 per channel (WHITE emission).
  RendererEmissiveBSDF mat;
  BackgroundColour bg(Colour(0.0f, 0.0f, 0.0f));
  Renderer r;
  setupEmissiveScene(r, mat, bg);

  r.render();

  float totalR = 0.0f;
  float totalG = 0.0f;
  float totalB = 0.0f;
  for (const Colour& c : r.film.film) {
    totalR += c.r;
    totalG += c.g;
    totalB += c.b;
  }
  float totalPixels = (float)(r.film.width * r.film.height);
  REQUIRE(totalR / totalPixels == Catch::Approx(1.0f).margin(0.001f));
  REQUIRE(totalG / totalPixels == Catch::Approx(1.0f).margin(0.001f));
  REQUIRE(totalB / totalPixels == Catch::Approx(1.0f).margin(0.001f));
}

TEST_CASE("Renderer render: raw film values double after second progressive render call") {
  // Depth-0 emissive hit always returns WHITE regardless of random jitter.
  // Two renders → raw accumulation doubles; SPP=2.
  RendererEmissiveBSDF mat;
  BackgroundColour bg(Colour(0.0f, 0.0f, 0.0f));
  Renderer r;
  setupEmissiveScene(r, mat, bg);

  r.render();
  int cx = r.scene.width  / 2;
  int cy = r.scene.height / 2;
  Colour after1 = r.film.film[cy * r.film.width + cx];

  r.render();
  Colour after2 = r.film.film[cy * r.film.width + cx];

  REQUIRE(after2.r == Catch::Approx(after1.r * 2.0f).margin(1e-4f));
  REQUIRE(after2.g == Catch::Approx(after1.g * 2.0f).margin(1e-4f));
  REQUIRE(after2.b == Catch::Approx(after1.b * 2.0f).margin(1e-4f));
}

TEST_CASE("Renderer render: center pixel normalized by SPP equals emission colour") {
  // Full-screen WHITE emissive at depth=0 → pathTrace returns WHITE for every ray.
  // raw pixel / SPP must equal 1.0 (WHITE) regardless of thread count or jitter.
  RendererEmissiveBSDF mat;
  BackgroundColour bg(Colour(0.0f, 0.0f, 0.0f));
  Renderer r;
  setupEmissiveScene(r, mat, bg);

  r.render();

  int cx = r.scene.width  / 2;
  int cy = r.scene.height / 2;
  Colour raw = r.film.film[cy * r.film.width + cx];
  float perSampleR = raw.r / (float)r.film.SPP;
  float perSampleG = raw.g / (float)r.film.SPP;
  float perSampleB = raw.b / (float)r.film.SPP;
  REQUIRE(perSampleR == Catch::Approx(1.0f).margin(0.01f));
  REQUIRE(perSampleG == Catch::Approx(1.0f).margin(0.01f));
  REQUIRE(perSampleB == Catch::Approx(1.0f).margin(0.01f));
}

// -----------------------------------------------------------------------
// Convergence, self-intersection, and threading tests
//
// Scene: 32×32, Lambertian floor at y=0, small area light at y=2, black bg.
// Camera at (0,3,5) looking at origin, 45° FOV.
// The center ray hits (0,0,0) directly under the light → real MC variance.
// -----------------------------------------------------------------------

static constexpr int CONV_W = 32;
static constexpr int CONV_H = 32;

// White emissive BSDF for the area light in convergence tests.
class ConvergenceLightBSDF : public BSDF {
public:
  ConvergenceLightBSDF() { emission = Colour(1.0f, 1.0f, 1.0f); }
  Vec3   sample(const ShadingData&, Sampler*, Colour& w, float& pdf) override
  { w = Colour(1.0f, 1.0f, 1.0f); pdf = 1.0f; return Vec3(0.0f, 1.0f, 0.0f); }
  Colour evaluate(const ShadingData&, const Vec3&) override { return Colour(1.0f, 1.0f, 1.0f); }
  float  PDF(const ShadingData&, const Vec3&) override { return 1.0f; }
  bool   isPureSpecular() override { return false; }
  bool   isTwoSided() override { return true; }
  float  mask(const ShadingData&) override { return 1.0f; }
};

static void setupConvergenceScene(Renderer& r, DiffuseBSDF& floorBSDF, ConvergenceLightBSDF& lightBSDF, BackgroundColour& bg) {
  Vec3 up(0.0f, 1.0f, 0.0f);
  Vertex fv0(Vec3(-20.0f, 0.0f, -20.0f), up, 0.0f, 0.0f);
  Vertex fv1(Vec3( 20.0f, 0.0f, -20.0f), up, 1.0f, 0.0f);
  Vertex fv2(Vec3(-20.0f, 0.0f,  20.0f), up, 0.0f, 1.0f);
  Triangle floorTri; floorTri.init(fv0, fv1, fv2, 0);

  Vec3 down(0.0f, -1.0f, 0.0f);
  Vertex lv0(Vec3(-1.0f, 2.0f, -1.0f), down, 0.0f, 0.0f);
  Vertex lv1(Vec3( 1.0f, 2.0f, -1.0f), down, 1.0f, 0.0f);
  Vertex lv2(Vec3(-1.0f, 2.0f,  1.0f), down, 0.0f, 1.0f);
  Triangle lightTri; lightTri.init(lv0, lv1, lv2, 1);

  r.scene.init({floorTri, lightTri}, {&floorBSDF, &lightBSDF}, &bg);
  r.scene.build();
  r.scene.width  = CONV_W;
  r.scene.height = CONV_H;
  r.film.init(CONV_W, CONV_H);

  float aspect = (float)CONV_W / (float)CONV_H;
  Matrix P = Matrix::perspective(0.001f, 1000.0f, aspect, 45.0f);
  Matrix V = Matrix::lookAt(Vec3(0.0f, 3.0f, 5.0f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f)).invert();
  r.scene.camera.init(P, CONV_W, CONV_H);
  r.scene.camera.updateView(V);
}

// RMSE of (film / SPP) against reference pixel values.
static float computeRMSE(const Film& film, const std::vector<float>& refR, const std::vector<float>& refG, const std::vector<float>& refB) {
  float invSPP = film.SPP > 0 ? 1.0f / (float)film.SPP : 1.0f;
  float sum = 0.0f;
  int numPixels = (int)refR.size();
  for (int i = 0; i < numPixels; i++) {
    float dr = film.film[i].r * invSPP - refR[i];
    float dg = film.film[i].g * invSPP - refG[i];
    float db = film.film[i].b * invSPP - refB[i];
    sum += dr * dr + dg * dg + db * db;
  }
  return sqrtf(sum / (float)(3 * numPixels));
}

TEST_CASE("Convergence: per-pixel RMSE decreases monotonically with SPP") {
  // Run a reference render at high SPP, then compare RMSE at increasing SPP counts.
  // RMSE must be strictly decreasing: RMSE(8) > RMSE(32) > RMSE(128).
  Texture refTex; refTex.loadDefault();
  DiffuseBSDF refFloor(&refTex);
  ConvergenceLightBSDF refLight;
  BackgroundColour refBg(Colour(0.0f, 0.0f, 0.0f));
  Renderer refRenderer;
  setupConvergenceScene(refRenderer, refFloor, refLight, refBg);
  for (int i = 0; i < 512; i++) refRenderer.render();

  int numPixels = CONV_W * CONV_H;
  float refInvSPP = 1.0f / (float)refRenderer.film.SPP;
  std::vector<float> refR(numPixels), refG(numPixels), refB(numPixels);
  for (int i = 0; i < numPixels; i++) {
    refR[i] = refRenderer.film.film[i].r * refInvSPP;
    refG[i] = refRenderer.film.film[i].g * refInvSPP;
    refB[i] = refRenderer.film.film[i].b * refInvSPP;
  }

  Texture testTex; testTex.loadDefault();
  DiffuseBSDF testFloor(&testTex);
  ConvergenceLightBSDF testLight;
  BackgroundColour testBg(Colour(0.0f, 0.0f, 0.0f));
  Renderer testRenderer;
  setupConvergenceScene(testRenderer, testFloor, testLight, testBg);

  int targetSPPs[] = {8, 32, 128};
  float prevRMSE = 1e10f;
  for (int targetSPP : targetSPPs) {
    while (testRenderer.film.SPP < targetSPP) testRenderer.render();
    float rmse = computeRMSE(testRenderer.film, refR, refG, refB);
    REQUIRE(rmse < prevRMSE);
    prevRMSE = rmse;
  }
}

TEST_CASE("Convergence slope: log-log RMSE vs SPP slope is approximately negative one half") {
  // O(1/sqrt(N)) convergence: quadrupling SPP halves RMSE → slope = -0.5 in log-log.
  // Verify RMSE(32) / RMSE(8) and RMSE(128) / RMSE(32) are each in [0.3, 0.7].
  Texture refTex; refTex.loadDefault();
  DiffuseBSDF refFloor(&refTex);
  ConvergenceLightBSDF refLight;
  BackgroundColour refBg(Colour(0.0f, 0.0f, 0.0f));
  Renderer refRenderer;
  setupConvergenceScene(refRenderer, refFloor, refLight, refBg);
  for (int i = 0; i < 512; i++) refRenderer.render();

  int numPixels = CONV_W * CONV_H;
  float refInvSPP = 1.0f / (float)refRenderer.film.SPP;
  std::vector<float> refR(numPixels), refG(numPixels), refB(numPixels);
  for (int i = 0; i < numPixels; i++) {
    refR[i] = refRenderer.film.film[i].r * refInvSPP;
    refG[i] = refRenderer.film.film[i].g * refInvSPP;
    refB[i] = refRenderer.film.film[i].b * refInvSPP;
  }

  Texture testTex; testTex.loadDefault();
  DiffuseBSDF testFloor(&testTex);
  ConvergenceLightBSDF testLight;
  BackgroundColour testBg(Colour(0.0f, 0.0f, 0.0f));
  Renderer testRenderer;
  setupConvergenceScene(testRenderer, testFloor, testLight, testBg);

  while (testRenderer.film.SPP < 8)   testRenderer.render();
  float rmse8  = computeRMSE(testRenderer.film, refR, refG, refB);

  while (testRenderer.film.SPP < 32)  testRenderer.render();
  float rmse32 = computeRMSE(testRenderer.film, refR, refG, refB);

  while (testRenderer.film.SPP < 128) testRenderer.render();
  float rmse128 = computeRMSE(testRenderer.film, refR, refG, refB);

  if (rmse8 > 1e-6f && rmse32 > 1e-6f) {
    float ratio8to32   = rmse32  / rmse8;
    float ratio32to128 = rmse128 / rmse32;
    REQUIRE(ratio8to32   >= 0.3f);
    REQUIRE(ratio8to32   <= 0.7f);
    REQUIRE(ratio32to128 >= 0.3f);
    REQUIRE(ratio32to128 <= 0.7f);
  }
}

TEST_CASE("No self-intersection: all pixels within expected bounds after rendering") {
  // Self-intersection causes a bounce ray to immediately re-hit the same surface,
  // multiplying contributions and creating firefly values far above emission level.
  // For a scene with emission=1, all per-pixel values must stay well below 10.
  Texture tex; tex.loadDefault();
  DiffuseBSDF floorBSDF(&tex);
  ConvergenceLightBSDF lightBSDF;
  BackgroundColour bg(Colour(0.0f, 0.0f, 0.0f));
  Renderer r;
  setupConvergenceScene(r, floorBSDF, lightBSDF, bg);
  for (int i = 0; i < 16; i++) r.render();

  float invSPP = 1.0f / (float)r.film.SPP;
  for (const Colour& pixel : r.film.film) {
    REQUIRE(pixel.r * invSPP <= 10.0f);
    REQUIRE(pixel.g * invSPP <= 10.0f);
    REQUIRE(pixel.b * invSPP <= 10.0f);
    REQUIRE(pixel.r >= 0.0f);
    REQUIRE(pixel.g >= 0.0f);
    REQUIRE(pixel.b >= 0.0f);
  }
}

TEST_CASE("Threading: all film values are finite and non-negative after multithreaded render") {
  // A data race during tile accumulation would produce NaN or negative values.
  // Check every accumulated pixel is finite and non-negative.
  Texture tex; tex.loadDefault();
  DiffuseBSDF floorBSDF(&tex);
  ConvergenceLightBSDF lightBSDF;
  BackgroundColour bg(Colour(0.0f, 0.0f, 0.0f));
  Renderer r;
  setupConvergenceScene(r, floorBSDF, lightBSDF, bg);
  r.render();
  r.render();

  REQUIRE(r.film.SPP == 2);
  for (const Colour& pixel : r.film.film) {
    REQUIRE(std::isfinite(pixel.r));
    REQUIRE(std::isfinite(pixel.g));
    REQUIRE(std::isfinite(pixel.b));
    REQUIRE(pixel.r >= 0.0f);
    REQUIRE(pixel.g >= 0.0f);
    REQUIRE(pixel.b >= 0.0f);
  }
}
