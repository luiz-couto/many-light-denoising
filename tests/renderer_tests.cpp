#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "renderer.h"
#include "geometry.h"

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
