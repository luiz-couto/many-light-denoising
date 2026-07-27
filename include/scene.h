#ifndef SCENE_H
#define SCENE_H

#include "GEMLoader.h"
#include "material_loader.h"
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

  int width = 0;
  int height = 0;

  std::vector<Triangle> triangles;
  std::vector<Light*> lights;
  std::vector<BSDF*> materials;

  // Loads a scene from sceneName/scene.json: camera, meshes, background, BVH.
  void load(const std::string& sceneName);

  // Populates triangles, materials, and AABB from the provided geometry.
  void init(const std::vector<Triangle>& meshTriangles, const std::vector<BSDF*>& meshMaterials);

  // Overload that also wires up the background light.
  void init(const std::vector<Triangle>& meshTriangles, const std::vector<BSDF*>& meshMaterials, Light* background);

  // Builds the BVH and promotes emissive triangle materials to AreaLights.
  void build();

  bool visible(const Vec3& p1, const Vec3& p2);
  IntersectionData traverse(const Ray& ray);
  Light* sampleLight(Sampler* sampler, float& pmf);
  Light* sampleLightWeighted(Sampler* sampler, float& pmf);
  Colour emit(Triangle* light, const ShadingData& shadingData, const Vec3& wi);
  ShadingData calculateShadingData(IntersectionData intersection, const Ray& ray);
  float areaLightSelectionPDF(unsigned int triangleID) const;

private:
  void loadCamera(GEMLoader::GEMScene& gemscene);
  void loadMeshes(const std::string& sceneName, GEMLoader::GEMScene& gemscene, MaterialLoader& loader);
  void loadBackground(GEMLoader::GEMScene& gemscene, MaterialLoader& loader);
};

#endif // SCENE_H
