#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "camera.h"

// 90° FOV, 4:3, identity view
static Camera makeCamera(int w = 800, int h = 600) {
  Camera cam;
  cam.init(Matrix::perspective(0.1f, 100.0f, (float)w / (float)h, 90.0f), w, h);
  cam.updateView(Matrix());
  return cam;
}

// -----------------------------------------------------------------------
// init
// -----------------------------------------------------------------------

TEST_CASE("Camera init stores width and height") {
  Camera cam;
  cam.init(Matrix::perspective(0.1f, 100.0f, 4.0f / 3.0f, 90.0f), 800, 600);
  REQUIRE(cam.width  == Catch::Approx(800.0f));
  REQUIRE(cam.height == Catch::Approx(600.0f));
}

TEST_CASE("Camera init inverseProjectionMatrix is actual inverse") {
  Camera cam = makeCamera();
  Matrix result = cam.projectionMatrix * cam.inverseProjectionMatrix;
  for (int r = 0; r < 4; r++) {
    for (int c = 0; c < 4; c++) {
      float expected = (r == c) ? 1.0f : 0.0f;
      REQUIRE(result.a[r][c] == Catch::Approx(expected).margin(1e-4f));
    }
  }
}

// -----------------------------------------------------------------------
// areaFilm — bug regression: old formula used a[0][0]/a[1][1] as aspect,
// which equals 1/aspect_ratio, giving an area off by aspect^2.
// With fov=90 (t=1) and aspect=2:
//   a[0][0] = 0.5, a[1][1] = 1
//   correct:    wlens = 2/0.5 = 4, hlens = 2/1 = 2, area = 8
//   old buggy:  wlens = 2/1  = 2, aspect_code = 0.5, hlens = 1, area = 2
// -----------------------------------------------------------------------

TEST_CASE("Camera areaFilm correct for square aspect (fov=90, aspect=1)") {
  Camera cam;
  cam.init(Matrix::perspective(0.1f, 100.0f, 1.0f, 90.0f), 400, 400);
  // t=1, a[0][0]=1, a[1][1]=1 -> wlens=2, hlens=2, area=4
  REQUIRE(cam.areaFilm == Catch::Approx(4.0f).margin(1e-4f));
}

TEST_CASE("Camera areaFilm correct for 2:1 aspect (fov=90, aspect=2) — old formula gives 2, correct is 8") {
  Camera cam;
  cam.init(Matrix::perspective(0.1f, 100.0f, 2.0f, 90.0f), 400, 200);
  REQUIRE(cam.areaFilm == Catch::Approx(8.0f).margin(1e-4f));
  // Explicitly guard against the old buggy value
  REQUIRE_FALSE(cam.areaFilm == Catch::Approx(2.0f).margin(1e-4f));
}

// -----------------------------------------------------------------------
// updateView
// -----------------------------------------------------------------------

TEST_CASE("Camera updateView with identity: origin is (0,0,0)") {
  Camera cam = makeCamera();
  REQUIRE(cam.origin.x == Catch::Approx(0.0f));
  REQUIRE(cam.origin.y == Catch::Approx(0.0f));
  REQUIRE(cam.origin.z == Catch::Approx(0.0f));
}

TEST_CASE("Camera updateView with translation: origin matches camera position") {
  Camera cam;
  cam.init(Matrix::perspective(0.1f, 100.0f, 1.0f, 90.0f), 400, 400);
  cam.updateView(Matrix::translation(Vec3(3.0f, -1.0f, 5.0f)));
  REQUIRE(cam.origin.x == Catch::Approx(3.0f));
  REQUIRE(cam.origin.y == Catch::Approx(-1.0f));
  REQUIRE(cam.origin.z == Catch::Approx(5.0f));
}

TEST_CASE("Camera updateView: viewDirection is unit length") {
  Camera cam = makeCamera();
  REQUIRE(cam.viewDirection.length() == Catch::Approx(1.0f).margin(1e-5f));
}

TEST_CASE("Camera updateView: cameraToView is inverse of camera matrix") {
  Camera cam;
  cam.init(Matrix::perspective(0.1f, 100.0f, 1.0f, 90.0f), 400, 400);
  cam.updateView(Matrix::translation(Vec3(1.0f, 2.0f, -3.0f)));
  Matrix result = cam.camera * cam.cameraToView;
  for (int r = 0; r < 4; r++) {
    for (int c = 0; c < 4; c++) {
      float expected = (r == c) ? 1.0f : 0.0f;
      REQUIRE(result.a[r][c] == Catch::Approx(expected).margin(1e-4f));
    }
  }
}

// -----------------------------------------------------------------------
// generateRay
// -----------------------------------------------------------------------

TEST_CASE("Camera generateRay: ray origin is camera origin") {
  Camera cam = makeCamera();
  Ray ray = cam.generateRay(400.0f, 300.0f);
  REQUIRE(ray.o.x == Catch::Approx(cam.origin.x));
  REQUIRE(ray.o.y == Catch::Approx(cam.origin.y));
  REQUIRE(ray.o.z == Catch::Approx(cam.origin.z));
}

TEST_CASE("Camera generateRay: ray direction is unit length") {
  Camera cam = makeCamera();
  Ray ray = cam.generateRay(400.0f, 300.0f);
  REQUIRE(ray.dir.length() == Catch::Approx(1.0f).margin(1e-5f));
}

TEST_CASE("Camera generateRay: center pixel direction matches viewDirection") {
  // Use odd dimensions so (width-1)/2 and (height-1)/2 are integers,
  // mapping exactly to NDC (0, 0).
  Camera cam;
  cam.init(Matrix::perspective(0.1f, 100.0f, 1.0f, 90.0f), 801, 801);
  cam.updateView(Matrix());
  Ray ray = cam.generateRay(400.0f, 400.0f);
  REQUIRE(ray.dir.x == Catch::Approx(cam.viewDirection.x).margin(1e-5f));
  REQUIRE(ray.dir.y == Catch::Approx(cam.viewDirection.y).margin(1e-5f));
  REQUIRE(ray.dir.z == Catch::Approx(cam.viewDirection.z).margin(1e-5f));
}

TEST_CASE("Camera generateRay: left and right pixels symmetric for identity camera") {
  // For a camera on the z-axis with symmetric FOV, pixels equidistant from
  // center should produce rays with negated x components and equal y/z.
  Camera cam;
  cam.init(Matrix::perspective(0.1f, 100.0f, 1.0f, 90.0f), 801, 801);
  cam.updateView(Matrix());
  Ray left  = cam.generateRay(100.0f, 400.0f);
  Ray right = cam.generateRay(700.0f, 400.0f);
  REQUIRE(left.dir.x  == Catch::Approx(-right.dir.x).margin(1e-5f));
  REQUIRE(left.dir.y  == Catch::Approx( right.dir.y).margin(1e-5f));
  REQUIRE(left.dir.z  == Catch::Approx( right.dir.z).margin(1e-5f));
}

// -----------------------------------------------------------------------
// projectOntoCamera
// -----------------------------------------------------------------------

TEST_CASE("Camera projectOntoCamera: point behind camera returns false") {
  Camera cam = makeCamera();
  float x, y;
  // Camera looks down -z; z=+10 is behind it. Without the pview.z>=0 guard,
  // w_clip would be negative and the perspective divide would produce valid-looking
  // screen coords (or silently mirror off-axis points to the wrong side).
  REQUIRE_FALSE(cam.projectOntoCamera(Vec3(0.0f, 0.0f, 10.0f), x, y));
}

TEST_CASE("Camera projectOntoCamera: off-axis point behind camera is not mirrored to screen") {
  Camera cam = makeCamera();
  float x, y;
  // Without the fix, p=(-5,0,10) would project to x≈0.69 (right side) — mirrored.
  REQUIRE_FALSE(cam.projectOntoCamera(Vec3(-5.0f, 0.0f, 10.0f), x, y));
}

TEST_CASE("Camera projectOntoCamera: point outside frustum to the side returns false") {
  Camera cam = makeCamera();
  float x, y;
  REQUIRE_FALSE(cam.projectOntoCamera(Vec3(1000.0f, 0.0f, -5.0f), x, y));
}

TEST_CASE("Camera projectOntoCamera: point outside frustum above returns false") {
  Camera cam = makeCamera();
  float x, y;
  REQUIRE_FALSE(cam.projectOntoCamera(Vec3(0.0f, 1000.0f, -5.0f), x, y));
}

TEST_CASE("Camera projectOntoCamera: point on view axis projects near film centre") {
  // A point along the centre NDC ray (0,0) should map to roughly (width/2, height/2)
  Camera cam;
  cam.init(Matrix::perspective(0.1f, 100.0f, 1.0f, 90.0f), 801, 801);
  cam.updateView(Matrix());
  // Generate center ray and pick a point along it
  Ray ray = cam.generateRay(400.0f, 400.0f);
  Vec3 p  = ray.at(5.0f);
  float x, y;
  REQUIRE(cam.projectOntoCamera(p, x, y));
  REQUIRE(x == Catch::Approx(400.5f).margin(1.0f));
  REQUIRE(y == Catch::Approx(400.5f).margin(1.0f));
}

TEST_CASE("Camera projectOntoCamera round-trip with generateRay is consistent within 1 pixel") {
  // generateRay uses (width-1) for NDC mapping; projectOntoCamera uses width.
  // The two conventions introduce a sub-pixel offset — this test confirms it stays < 1 pixel.
  Camera cam;
  cam.init(Matrix::perspective(0.1f, 100.0f, 1.0f, 90.0f), 801, 801);
  cam.updateView(Matrix());

  float px = 200.0f, py = 600.0f;
  Ray ray  = cam.generateRay(px, py);
  Vec3 p   = ray.at(5.0f);
  float rx, ry;
  REQUIRE(cam.projectOntoCamera(p, rx, ry));
  REQUIRE(rx == Catch::Approx(px).margin(1.0f));
  REQUIRE(ry == Catch::Approx(py).margin(1.0f));
}
