#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cfloat>
#include "scene.h"

// -----------------------------------------------------------------------
// Minimal concrete stubs for abstract interfaces
// -----------------------------------------------------------------------

struct MockBSDF : BSDF {
  bool twoSided;
  explicit MockBSDF(bool ts = false) : twoSided(ts) {}
  Vec3    sample(const ShadingData&, Sampler*, Colour&, float&) override { return Vec3(); }
  Colour  evaluate(const ShadingData&, const Vec3&)             override { return Colour(); }
  float   PDF(const ShadingData&, const Vec3&)                  override { return 0.0f; }
  bool    isPureSpecular()                                       override { return false; }
  bool    isTwoSided()                                          override { return twoSided; }
  float   mask(const ShadingData&)                              override { return 1.0f; }
};

struct MockLight : Light {
  float power;
  explicit MockLight(float p = 0.0f) : power(p) {}
  Vec3   sample(const ShadingData&, Sampler*, Colour&, float&)  override { return Vec3(); }
  Colour evaluate(const Vec3&)                                   override { return Colour(); }
  float  PDF(const ShadingData&, const Vec3&)                   override { return 0.0f; }
  bool   isArea()                                                override { return false; }
  Vec3   normal(const ShadingData&, const Vec3&)                override { return Vec3(); }
  float  totalIntegratedPower()                                  override { return power; }
  Vec3   samplePositionFromLight(Sampler*, float&)              override { return Vec3(); }
  Vec3   sampleDirectionFromLight(Sampler*, float&)             override { return Vec3(); }
};

// Sampler that always returns a fixed value — used to test boundary conditions.
struct ConstSampler : Sampler {
  float value;
  explicit ConstSampler(float v) : value(v) {}
  float next() override { return value; }
};

// -----------------------------------------------------------------------
// Scene / triangle helpers
// -----------------------------------------------------------------------

static Triangle makeTri(Vec3 v0, Vec3 v1, Vec3 v2, unsigned int matIdx = 0) {
  Vec3 n = (v1 - v0).cross(v2 - v0).normalize();
  Triangle t;
  t.init(Vertex(v0, n, 0.0f, 0.0f),
         Vertex(v1, n, 1.0f, 0.0f),
         Vertex(v2, n, 0.0f, 1.0f), matIdx);
  return t;
}

// One triangle in the XY plane at z=0 facing +z, one non-two-sided material,
// zero-power background (not added to lights list).
static Scene makeBasicScene(bool twoSided = false) {
  Triangle tri = makeTri(Vec3(0,0,0), Vec3(1,0,0), Vec3(0,1,0), 0);
  MockBSDF*  bsdf = new MockBSDF(twoSided);
  MockLight* bg   = new MockLight(0.0f);
  Scene scene;
  scene.init({tri}, {bsdf}, bg);
  scene.build();
  return scene;
}

// -----------------------------------------------------------------------
// init
// -----------------------------------------------------------------------

TEST_CASE("Scene init: triangles are stored") {
  Triangle tri = makeTri(Vec3(0,0,0), Vec3(1,0,0), Vec3(0,1,0));
  MockBSDF* bsdf = new MockBSDF();
  MockLight* bg  = new MockLight(0.0f);
  Scene scene;
  scene.init({tri}, {bsdf}, bg);
  REQUIRE(scene.triangles.size() == 1);
}

TEST_CASE("Scene init: materials are stored") {
  Triangle tri = makeTri(Vec3(0,0,0), Vec3(1,0,0), Vec3(0,1,0));
  MockBSDF* bsdf = new MockBSDF();
  MockLight* bg  = new MockLight(0.0f);
  Scene scene;
  scene.init({tri}, {bsdf}, bg);
  REQUIRE(scene.materials.size() == 1);
  REQUIRE(scene.materials[0] == bsdf);
}

TEST_CASE("Scene init: bounds cover all vertices") {
  Triangle tri = makeTri(Vec3(0,0,0), Vec3(2,0,0), Vec3(0,3,0));
  MockBSDF* bsdf = new MockBSDF();
  MockLight* bg  = new MockLight(0.0f);
  Scene scene;
  scene.init({tri}, {bsdf}, bg);
  REQUIRE(scene.bounds.bmax.x == Catch::Approx(2.0f));
  REQUIRE(scene.bounds.bmax.y == Catch::Approx(3.0f));
  REQUIRE(scene.bounds.bmin.x == Catch::Approx(0.0f));
}

TEST_CASE("Scene init: background with power > 0 is added to lights") {
  Triangle tri = makeTri(Vec3(0,0,0), Vec3(1,0,0), Vec3(0,1,0));
  MockBSDF* bsdf = new MockBSDF();
  MockLight* bg  = new MockLight(5.0f);
  Scene scene;
  scene.init({tri}, {bsdf}, bg);
  REQUIRE(scene.lights.size() == 1);
  REQUIRE(scene.lights[0] == bg);
}

TEST_CASE("Scene init: background with zero power is not added to lights") {
  Triangle tri = makeTri(Vec3(0,0,0), Vec3(1,0,0), Vec3(0,1,0));
  MockBSDF* bsdf = new MockBSDF();
  MockLight* bg  = new MockLight(0.0f);
  Scene scene;
  scene.init({tri}, {bsdf}, bg);
  REQUIRE(scene.lights.empty());
}

// -----------------------------------------------------------------------
// build
// -----------------------------------------------------------------------

TEST_CASE("Scene build: BVH has nodes after build") {
  Scene scene = makeBasicScene();
  REQUIRE_FALSE(scene.bvh.nodes.empty());
}

// -----------------------------------------------------------------------
// traverse
// -----------------------------------------------------------------------

TEST_CASE("Scene traverse: ray hit returns t < FLT_MAX and correct triangle ID") {
  Scene scene = makeBasicScene();
  // Ray from above the triangle center, going down
  Ray ray(Vec3(0.25f, 0.25f, 1.0f), Vec3(0.0f, 0.0f, -1.0f));
  IntersectionData inter = scene.traverse(ray);
  REQUIRE(inter.t < FLT_MAX);
  REQUIRE(inter.ID == 0);
}

TEST_CASE("Scene traverse: ray miss returns t == FLT_MAX") {
  Scene scene = makeBasicScene();
  // Ray going away from the triangle
  Ray ray(Vec3(0.25f, 0.25f, 1.0f), Vec3(0.0f, 0.0f, 1.0f));
  IntersectionData inter = scene.traverse(ray);
  REQUIRE(inter.t == FLT_MAX);
}

// -----------------------------------------------------------------------
// visible
// -----------------------------------------------------------------------

TEST_CASE("Scene visible: unobstructed segment returns true") {
  Scene scene = makeBasicScene();
  // Both points above the triangle — nothing between them
  Vec3 p1(0.25f, 0.25f, 1.0f);
  Vec3 p2(0.25f, 0.25f, 2.0f);
  REQUIRE(scene.visible(p1, p2));
}

TEST_CASE("Scene visible: obstructed segment returns false") {
  Scene scene = makeBasicScene();
  // Triangle is at z=0; p1 above and p2 below — triangle blocks the path
  Vec3 p1(0.25f, 0.25f,  1.0f);
  Vec3 p2(0.25f, 0.25f, -1.0f);
  REQUIRE_FALSE(scene.visible(p1, p2));
}

TEST_CASE("Scene visible: points closer than 2*RAY_EPSILON apart return true") {
  Scene scene = makeBasicScene();
  // maxT goes negative — traverseVisibleNode never fires, so result is visible
  Vec3 p1(0.25f, 0.25f, 0.5f);
  Vec3 p2 = p1 + Vec3(0.0f, 0.0f, 1e-7f);
  REQUIRE(scene.visible(p1, p2));
}

// -----------------------------------------------------------------------
// sampleLight
// -----------------------------------------------------------------------

TEST_CASE("Scene sampleLight: returns a valid light and correct pmf") {
  Scene scene = makeBasicScene();
  MockLight* l1 = new MockLight(1.0f);
  MockLight* l2 = new MockLight(1.0f);
  scene.lights.push_back(l1);
  scene.lights.push_back(l2);

  MTRandom sampler;
  float pmf;
  Light* chosen = scene.sampleLight(&sampler, pmf);
  REQUIRE(chosen != nullptr);
  REQUIRE(pmf == Catch::Approx(0.5f));
}

TEST_CASE("Scene sampleLight: sampler returning 1.0 does not go out of bounds") {
  // Without the std::min clamp, (int)(1.0 * N) == N which is out of bounds.
  Scene scene = makeBasicScene();
  MockLight* l1 = new MockLight(1.0f);
  MockLight* l2 = new MockLight(1.0f);
  scene.lights.push_back(l1);
  scene.lights.push_back(l2);

  ConstSampler sampler(1.0f);
  float pmf;
  Light* chosen = scene.sampleLight(&sampler, pmf);
  REQUIRE(chosen != nullptr);
  REQUIRE((chosen == l1 || chosen == l2));
}

// -----------------------------------------------------------------------
// sampleLightWeighted
// -----------------------------------------------------------------------

TEST_CASE("Scene sampleLightWeighted: all zero power falls back to uniform") {
  Scene scene = makeBasicScene();
  MockLight* l1 = new MockLight(0.0f);
  MockLight* l2 = new MockLight(0.0f);
  scene.lights.push_back(l1);
  scene.lights.push_back(l2);

  MTRandom sampler;
  float pmf;
  Light* chosen = scene.sampleLightWeighted(&sampler, pmf);
  REQUIRE(chosen != nullptr);
  REQUIRE(pmf == Catch::Approx(0.5f));
}

TEST_CASE("Scene sampleLightWeighted: zero-power light is never chosen") {
  Scene scene = makeBasicScene();
  MockLight* lit  = new MockLight(1.0f);
  MockLight* dark = new MockLight(0.0f);
  scene.lights.push_back(lit);
  scene.lights.push_back(dark);

  MTRandom sampler;
  for (int i = 0; i < 100; i++) {
    float pmf;
    REQUIRE(scene.sampleLightWeighted(&sampler, pmf) == lit);
  }
}

TEST_CASE("Scene sampleLightWeighted: pmf equals power fraction of chosen light") {
  Scene scene = makeBasicScene();
  MockLight* l1 = new MockLight(3.0f);
  MockLight* l2 = new MockLight(1.0f);
  scene.lights.push_back(l1);
  scene.lights.push_back(l2);

  // Force sampler to pick l1 (sample < 3) and l2 (sample in [3,4))
  ConstSampler pickFirst(0.0f);
  float pmf;
  Light* chosen = scene.sampleLightWeighted(&pickFirst, pmf);
  REQUIRE(chosen == l1);
  REQUIRE(pmf == Catch::Approx(3.0f / 4.0f));

  ConstSampler pickLast(0.9999f);
  chosen = scene.sampleLightWeighted(&pickLast, pmf);
  REQUIRE(chosen == l2);
  REQUIRE(pmf == Catch::Approx(1.0f / 4.0f));
}

TEST_CASE("Scene sampleLightWeighted: high-power light sampled proportionally") {
  Scene scene = makeBasicScene();
  MockLight* heavy = new MockLight(3.0f);
  MockLight* light = new MockLight(1.0f);
  scene.lights.push_back(heavy);
  scene.lights.push_back(light);

  MTRandom sampler;
  int heavyCount = 0;
  const int N = 10000;
  for (int i = 0; i < N; i++) {
    float pmf;
    if (scene.sampleLightWeighted(&sampler, pmf) == heavy) heavyCount++;
  }
  float ratio = (float)heavyCount / N;
  REQUIRE(ratio == Catch::Approx(0.75f).margin(0.03f));
}

// -----------------------------------------------------------------------
// calculateShadingData
// -----------------------------------------------------------------------

TEST_CASE("Scene calculateShadingData: miss sets wo and t = FLT_MAX, rest default") {
  Scene scene = makeBasicScene();
  Ray ray(Vec3(0,0,5), Vec3(0,0,-1));
  IntersectionData miss; // t = FLT_MAX by default
  ShadingData sd = scene.calculateShadingData(miss, ray);
  REQUIRE(sd.t == FLT_MAX);
  REQUIRE(sd.wo.z == Catch::Approx(1.0f)); // -(-1) = (0,0,1)
  REQUIRE(sd.x.x  == Catch::Approx(0.0f)); // x untouched — default zero
}

TEST_CASE("Scene calculateShadingData: hit sets x on triangle surface") {
  Scene scene = makeBasicScene();
  Ray ray(Vec3(0.25f, 0.25f, 1.0f), Vec3(0.0f, 0.0f, -1.0f));
  IntersectionData inter = scene.traverse(ray);
  ShadingData sd = scene.calculateShadingData(inter, ray);
  REQUIRE(sd.t   == Catch::Approx(inter.t).margin(1e-4f));
  REQUIRE(sd.x.x == Catch::Approx(0.25f).margin(1e-4f));
  REQUIRE(sd.x.y == Catch::Approx(0.25f).margin(1e-4f));
  REQUIRE(sd.x.z == Catch::Approx(0.0f).margin(1e-4f));
}

TEST_CASE("Scene calculateShadingData: hit sets wo as negated ray direction") {
  Scene scene = makeBasicScene();
  Ray ray(Vec3(0.25f, 0.25f, 1.0f), Vec3(0.0f, 0.0f, -1.0f));
  IntersectionData inter = scene.traverse(ray);
  ShadingData sd = scene.calculateShadingData(inter, ray);
  REQUIRE(sd.wo.z == Catch::Approx(1.0f)); // -(0,0,-1) = (0,0,1)
}

TEST_CASE("Scene calculateShadingData: hit sets gNormal from triangle") {
  Scene scene = makeBasicScene();
  Ray ray(Vec3(0.25f, 0.25f, 1.0f), Vec3(0.0f, 0.0f, -1.0f));
  IntersectionData inter = scene.traverse(ray);
  ShadingData sd = scene.calculateShadingData(inter, ray);
  // Triangle in XY plane facing +z
  REQUIRE(sd.gNormal.z == Catch::Approx(1.0f).margin(1e-4f));
}

TEST_CASE("Scene calculateShadingData: hit sets bsdf pointer") {
  Scene scene = makeBasicScene();
  Ray ray(Vec3(0.25f, 0.25f, 1.0f), Vec3(0.0f, 0.0f, -1.0f));
  IntersectionData inter = scene.traverse(ray);
  ShadingData sd = scene.calculateShadingData(inter, ray);
  REQUIRE(sd.bsdf == scene.materials[0]);
}

TEST_CASE("Scene calculateShadingData: two-sided material flips normal when wo on wrong side") {
  Scene scene = makeBasicScene(true); // twoSided = true
  // Ray from below (+z direction) hits back face: wo = (0,0,-1), gNormal = (0,0,1) -> dot < 0 -> flip
  Ray ray(Vec3(0.25f, 0.25f, -1.0f), Vec3(0.0f, 0.0f, 1.0f));
  IntersectionData inter = scene.traverse(ray);
  REQUIRE(inter.t < FLT_MAX);
  ShadingData sd = scene.calculateShadingData(inter, ray);
  REQUIRE(sd.gNormal.z == Catch::Approx(-1.0f).margin(1e-4f));
  REQUIRE(sd.sNormal.z == Catch::Approx(-1.0f).margin(1e-4f));
}

TEST_CASE("Scene calculateShadingData: one-sided material does not flip normal") {
  Scene scene = makeBasicScene(false); // twoSided = false
  Ray ray(Vec3(0.25f, 0.25f, -1.0f), Vec3(0.0f, 0.0f, 1.0f));
  IntersectionData inter = scene.traverse(ray);
  REQUIRE(inter.t < FLT_MAX);
  ShadingData sd = scene.calculateShadingData(inter, ray);
  REQUIRE(sd.gNormal.z == Catch::Approx(1.0f).margin(1e-4f));
}
