#ifndef CAMERA_H
#define CAMERA_H

#include "core.h"
#include "ray.h"

class Camera {
public:
	Matrix projectionMatrix;
	Matrix inverseProjectionMatrix;
	Matrix camera;
	Matrix cameraToView;

	Vec3 origin;
	Vec3 viewDirection;

	float width = 0;
	float height = 0;
	float areaFilm;

	Camera();
	void init(Matrix projectionMatrix, int screenwidth, int screenheight);
	void updateView(Matrix view);
	Ray generateRay(float x, float y);
	bool projectOntoCamera(const Vec3& p, float& x, float& y);
};

#endif // CAMERA_H
