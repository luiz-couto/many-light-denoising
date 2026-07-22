#include "camera.h"

Camera::Camera() {}

void Camera::init(Matrix projectionMatrix, int screenwidth, int screenheight) {
  width = (float)screenwidth;
  height = (float)screenheight;

  this->projectionMatrix = projectionMatrix;
  inverseProjectionMatrix = projectionMatrix.invert();

  float hlens = 2.0f / projectionMatrix.a[1][1];
  float wlens = 2.0f / projectionMatrix.a[0][0];
  areaFilm = wlens * hlens;
}

// Updates the camera's world-space transform from a new camera-to-world matrix.
// Stores the matrix and its inverse (world-to-camera), then derives two cached
// values used every frame: origin (camera position in world space, computed by
// transforming the camera-space origin through the matrix) and viewDirection
// (the canonical forward vector Vec3(0,0,1) unprojected from clip space into
// camera space, then rotated to world space and normalized).
void Camera::updateView(Matrix cameraToWorld) {
  camera = cameraToWorld;
  cameraToView = cameraToWorld.invert();
  origin = camera.mulPoint(Vec3(0, 0, 0));

  viewDirection = inverseProjectionMatrix.mulPointAndPerspectiveDivide(Vec3(0, 0, 1));
  viewDirection = camera.mulVec(viewDirection);
  viewDirection = viewDirection.normalize();
}

// Generates a world-space ray for pixel (x, y)
Ray Camera::generateRay(float x, float y) {
  float xc = ((2 * x) / (width - 1)) - 1;
  float yc = -(((2 * y) / (height - 1)) - 1);
  float zc = 0;

  Vec3 pclip(xc, yc, zc);
  Vec3 dCamera = inverseProjectionMatrix.mulPointAndPerspectiveDivide(pclip);

  Vec3 rayDir = camera.mulVec(dCamera).normalize();
  return Ray(origin, rayDir);
}

// Projects a world-space point onto the film, returning its pixel coordinates
// via x and y. Transforms the point from world space to camera space, then
// through the projection matrix into screen space. Remaps screen space [-1, 1] to [0, 1],
// rejects the point (returns false) if it falls outside the frustum, then scales to pixel
// coordinates and flips y to match screen-space convention (y grows downward).
// Returns true if the point is visible from the camera, false otherwise.
bool Camera::projectOntoCamera(const Vec3& p, float& x, float& y) {
  Vec3 pview = cameraToView.mulPoint(p);
  if (pview.z >= 0.0f) return false; // behind or on the camera plane (w_clip <= 0)
  
  Vec3 pproj = projectionMatrix.mulPointAndPerspectiveDivide(pview);

  x = (pproj.x + 1.0f) * 0.5f;
  y = (pproj.y + 1.0f) * 0.5f;

  if (x < 0 || x > 1.0f || y < 0 || y > 1.0f) {
    return false;
  }

  x = x * width;
  y = 1.0f - y;
  y = y * height;

  return true;
}
