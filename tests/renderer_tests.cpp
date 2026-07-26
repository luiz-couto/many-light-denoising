#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "renderer.h"
#include "geometry.h"
#include <atomic>
#include <cfloat>

// -----------------------------------------------------------------------
// Stubs
// -----------------------------------------------------------------------

class RendererMockBSDF : public BSDF {
public:
  Vec3 sample(const ShadingData&, Sampler*, Colour& w, float& pdf) override {
    w = Colour(1.0f, 1.0f, 1.0f); pdf = 1.0f; return Vec3(0, 0, 1);
  }
  Colour evaluate(const ShadingData&, const Vec3&) override { return Colour(1.0f, 1.0f, 1.0f); }
  float PDF(const ShadingData&, const Vec3&) override { return 1.0f; }
  bool isPureSpecular() override { return false; }
  bool isTwoSided() override { return true; }
  float mask(const ShadingData&) override { return 1.0f; }
};

class RendererMockLight : public Light {
public:
  Vec3 sample(const ShadingData&, Sampler*, Colour& e, float& pdf) override {
    e = Colour(0.0f, 0.0f, 0.0f); pdf = 1.0f; return Vec3(0, 1, 0);
  }
  Colour evaluate(const Vec3&) override { return Colour(0.0f, 0.0f, 0.0f); }
  float PDF(const ShadingData&, const Vec3&) override { return 0.0f; }
  bool isArea() override { return false; }
  Vec3 normal(const ShadingData&, const Vec3&) override { return Vec3(0, 1, 0); }
  float totalIntegratedPower() override { return 0.0f; }
  Vec3 samplePositionFromLight(Sampler*, float& pdf) override { pdf = 1.0f; return Vec3(0, 0, 0); }
  Vec3 sampleDirectionFromLight(Sampler*, float& pdf) override { pdf = 1.0f; return Vec3(0, 1, 0); }
};

static constexpr int TEST_W = 800;
static constexpr int TEST_H = 600;

// Large +Z triangle that covers the entire camera frustum.
// Camera at (0,0,5) looking at origin, FOV 45, aspect 800/600.
// At z=0 the frustum is ~±2.1 units tall — triangle vertices at ±10 cover it fully.
static void setupScene(Renderer& r, RendererMockBSDF& mat, RendererMockLight& bg) {
  Vertex v0(Vec3(-10.f, -10.f, 0.f), Vec3(0.f, 0.f, 1.f), 0.f, 0.f);
  Vertex v1(Vec3( 10.f, -10.f, 0.f), Vec3(0.f, 0.f, 1.f), 1.f, 0.f);
  Vertex v2(Vec3(  0.f,  10.f, 0.f), Vec3(0.f, 0.f, 1.f), 0.5f, 1.f);
  Triangle tri;
  tri.init(v0, v1, v2, 0);

  r.scene.init({tri}, {&mat}, &bg);
  r.scene.build();

  r.scene.width  = TEST_W;
  r.scene.height = TEST_H;
  r.film.init(TEST_W, TEST_H);

  float aspect = (float)TEST_W / (float)TEST_H;
  Matrix P = Matrix::perspective(0.001f, 1000.0f, aspect, 45.0f);
  Matrix V = Matrix::lookAt(Vec3(0, 0, 5), Vec3(0, 0, 0), Vec3(0, 1, 0)).invert();
  r.scene.camera.init(P, TEST_W, TEST_H);
  r.scene.camera.updateView(V);
}

// -----------------------------------------------------------------------
// renderTile
// -----------------------------------------------------------------------

TEST_CASE("Renderer renderTile: +Z triangle gives (0.5, 0.5, 1.0) at center pixel") {
  RendererMockBSDF mat;
  RendererMockLight bg;
  Renderer r;
  setupScene(r, mat, bg);

  std::atomic<unsigned int> tileId(0);
  MTRandom sampler;
  r.renderTile(0, tileId, sampler);

  int cx = r.scene.width  / 2;
  int cy = r.scene.height / 2;
  Colour& px = r.film.film[cy * r.film.width + cx];
  REQUIRE(px.r == Catch::Approx(0.5f).margin(0.01f));
  REQUIRE(px.g == Catch::Approx(0.5f).margin(0.01f));
  REQUIRE(px.b == Catch::Approx(1.0f).margin(0.01f));
}

TEST_CASE("Renderer renderTile: out-of-range tileId does nothing") {
  RendererMockBSDF mat;
  RendererMockLight bg;
  Renderer r;
  setupScene(r, mat, bg);

  int tilesX = (r.scene.width  + Config::TILE_SIZE - 1) / Config::TILE_SIZE;
  int tilesY = (r.scene.height + Config::TILE_SIZE - 1) / Config::TILE_SIZE;
  std::atomic<unsigned int> tileId((unsigned int)(tilesX * tilesY));
  MTRandom sampler;
  r.renderTile(0, tileId, sampler);

  for (const Colour& c : r.film.film) {
    REQUIRE(c.r == Catch::Approx(0.0f));
    REQUIRE(c.g == Catch::Approx(0.0f));
    REQUIRE(c.b == Catch::Approx(0.0f));
  }
}

TEST_CASE("Renderer renderTile: all tiles exhausted after single-thread run") {
  RendererMockBSDF mat;
  RendererMockLight bg;
  Renderer r;
  setupScene(r, mat, bg);

  int tilesX = (r.scene.width  + Config::TILE_SIZE - 1) / Config::TILE_SIZE;
  int tilesY = (r.scene.height + Config::TILE_SIZE - 1) / Config::TILE_SIZE;
  int totalTiles = tilesX * tilesY;

  std::atomic<unsigned int> tileId(0);
  MTRandom sampler;
  r.renderTile(0, tileId, sampler);

  REQUIRE((int)tileId.load() >= totalTiles);
}

// -----------------------------------------------------------------------
// render()
// -----------------------------------------------------------------------

TEST_CASE("Renderer render: SPP is 1 after single render call") {
  RendererMockBSDF mat;
  RendererMockLight bg;
  Renderer r;
  setupScene(r, mat, bg);

  r.render();

  REQUIRE(r.film.SPP == 1);
}

TEST_CASE("Renderer render: film has non-zero values after render with geometry") {
  RendererMockBSDF mat;
  RendererMockLight bg;
  Renderer r;
  setupScene(r, mat, bg);

  r.render();

  int cx = r.scene.width  / 2;
  int cy = r.scene.height / 2;
  Colour px = r.film.film[cy * r.film.width + cx];
  REQUIRE(px.b > 0.0f);
}

TEST_CASE("Renderer render: film is cleared before each render call") {
  RendererMockBSDF mat;
  RendererMockLight bg;
  Renderer r;
  setupScene(r, mat, bg);

  r.render();
  r.render();

  // render() calls film.clear() first so SPP resets and increments once — not twice
  REQUIRE(r.film.SPP == 1);
}

// -----------------------------------------------------------------------
// Threading correctness
// -----------------------------------------------------------------------

TEST_CASE("Renderer render: same result regardless of thread count") {
  // Normal shading is deterministic — no randomness — so N threads must
  // produce the same film values as 1 thread.
  RendererMockBSDF mat;
  RendererMockLight bg;

  // Single-threaded: run all tiles from one thread
  Renderer r1;
  setupScene(r1, mat, bg);
  {
    std::atomic<unsigned int> tileId(0);
    MTRandom sampler;
    r1.renderTile(0, tileId, sampler);
  }

  // Multi-threaded: use render() which spawns hardware_concurrency threads
  Renderer r2;
  setupScene(r2, mat, bg);
  r2.render();

  int cx = r1.scene.width  / 2;
  int cy = r1.scene.height / 2;
  Colour p1 = r1.film.film[cy * r1.film.width + cx];
  Colour p2 = r2.film.film[cy * r2.film.width + cx];

  REQUIRE(p1.r == Catch::Approx(p2.r).margin(0.001f));
  REQUIRE(p1.g == Catch::Approx(p2.g).margin(0.001f));
  REQUIRE(p1.b == Catch::Approx(p2.b).margin(0.001f));
}
