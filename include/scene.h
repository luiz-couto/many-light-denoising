#ifndef SCENE_H
#define SCENE_H

#include "geometry.h"
#include "bvh.h"
#include "camera.h"
#include "shading.h"
#include "light.h"

class Scene {
public:
	BVH bvh;
	Light* background = nullptr;
	Camera camera;
	AABB bounds;

	std::vector<Triangle> triangles;
	std::vector<Light*> lights;
	std::vector<BSDF*> materials;

  Scene();

	// Populates triangles and materials from the loaded mesh, expands the scene
	// AABB to cover all vertices, stores the background light, and adds it to
	// the light list if its integrated power is greater than zero.
	void init(const std::vector<Triangle>& meshTriangles, const std::vector<BSDF*>& meshMaterials, Light* background);

	// Builds the BVH over all triangles. Then walks every triangle and promotes
	// any whose material isLight() into an AreaLight, appending it to lights.
	void build();

	// Returns true if p1 and p2 are mutually visible. Constructs a shadow ray
	// offset by RAY_EPSILON from p1 toward p2 with maxT set just short of p2,
	// then delegates to bvh.traverseVisible to test for occluders.
	bool visible(const Vec3& p1, const Vec3& p2);

	// Returns the closest intersection of ray with the scene by delegating to
	// bvh.traverse. IntersectionData.t == FLT_MAX indicates a miss.
	IntersectionData traverse(const Ray& ray);

	// Returns a uniformly chosen light and sets pmf = 1 / lights.size().
	Light* sampleLight(Sampler* sampler, float& pmf);

	// Returns a light sampled proportionally to its totalIntegratedPower via
	// CDF traversal, setting pmf to the chosen light's power fraction. Falls
	// back to uniform sampling if total power is zero.
	Light* sampleLightWeighted(Sampler* sampler, float& pmf);

	// Queries the emissive material of a light triangle for the radiance it
	// emits toward direction wi from the given shading point.
	Colour emit(Triangle* light, const ShadingData& shadingData, const Vec3& wi);

	// Converts raw intersection data and the originating ray into a fully
	// populated ShadingData. On a hit: computes the world-space hit point,
	// geometric normal, interpolated shading normal and UVs, BSDF pointer,
	// outgoing direction wo, flips normals for two-sided materials if wo is
	// on the wrong side, and builds the shading Frame from sNormal.
	// On a miss: sets only wo and t = FLT_MAX, leaving all other fields default.
	ShadingData calculateShadingData(IntersectionData intersection, const Ray& ray);
};

#endif // SCENE_H
