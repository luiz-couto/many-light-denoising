#include "scene.h"
#include <algorithm>

void Scene::load(const std::string& sceneName) {
  GEMLoader::GEMScene gemscene;
  gemscene.load(sceneName + "/scene.json");

  MaterialLoader loader(sceneName);
  loadCamera(gemscene);
  loadMeshes(sceneName, gemscene, loader);
  loadBackground(gemscene, loader);

  build();
}

void Scene::loadCamera(GEMLoader::GEMScene& gemscene) {
  width  = gemscene.findProperty("width").getValue(1920);
  height = gemscene.findProperty("height").getValue(1080);
  float fov  = gemscene.findProperty("fov").getValue(45.0f);

  Vec3 from, to, up;
  gemscene.findProperty("from").getValuesAsVector3(from.x, from.y, from.z);
  gemscene.findProperty("to").getValuesAsVector3(to.x, to.y, to.z);
  gemscene.findProperty("up").getValuesAsVector3(up.x, up.y, up.z);

  Matrix P = Matrix::perspective(0.001f, 10000.0f, (float)width / (float)height, fov);
  if (gemscene.findProperty("flipX").getValue(0) == 1) P.a[0][0] = -P.a[0][0];

  Matrix V = Matrix::lookAt(from, to, up);
  V = V.invert();

  camera.init(P, width, height);
  camera.updateView(V);
}

void Scene::loadMeshes(const std::string& sceneName, GEMLoader::GEMScene& gemscene, MaterialLoader& loader) {
  std::vector<Triangle> meshTriangles;
  std::vector<BSDF*> meshMaterials;

  for (auto& instance : gemscene.instances) {
    BSDF* material = loader.load(instance);
    if (!material) continue;

    meshMaterials.push_back(material);
    int materialIndex = (int)meshMaterials.size() - 1;

    GEMLoader::GEMModelLoader modelLoader;
    std::vector<GEMLoader::GEMMesh> meshes;
    modelLoader.load(sceneName + "/" + instance.meshFilename, meshes);

    Matrix transform;
    memcpy(transform.m, instance.w.m, 16 * sizeof(float));
    Matrix vecTransform = transform.invert();
    vecTransform = vecTransform.transpose();

    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    for (auto& mesh : meshes) {
      int vertexOffset = (int)vertices.size();
      for (auto& vs : mesh.verticesStatic) {
        Vertex v;
        v.p = transform.mulPoint(Vec3(vs.position.x, vs.position.y, vs.position.z));
        v.normal = vecTransform.mulVec(Vec3(vs.normal.x, vs.normal.y, vs.normal.z)).normalize();
        v.u = vs.u;
        v.v = vs.v;
        vertices.push_back(v);
      }
      for (auto idx : mesh.indices) indices.push_back(vertexOffset + idx);
    }

    for (int i = 0; i < (int)indices.size(); i += 3) {
      Triangle t;
      t.init(vertices[indices[i]], vertices[indices[i + 1]], vertices[indices[i + 2]], materialIndex);
      if (t.area > 0) meshTriangles.push_back(t);
    }
  }

  init(meshTriangles, meshMaterials);
}

void Scene::loadBackground(GEMLoader::GEMScene& gemscene, MaterialLoader& loader) {
  Vec3 centre = (bounds.bmax + bounds.bmin) * 0.5f;
  float radius = (bounds.bmax - centre).length();

  std::string envmapPath = gemscene.findProperty("envmap").getValue("");
  if (!envmapPath.empty()) {
    background = new EnvironmentMap(loader.loadTexture(envmapPath), centre, radius);
  } else {
    background = new BackgroundColour(Colour(0.0f, 0.0f, 0.0f));
  }

  if (background->totalIntegratedPower() > 0) lights.push_back(background);
}

void Scene::init(const std::vector<Triangle>& meshTriangles, const std::vector<BSDF*>& meshMaterials) {
  for (const auto& tri : meshTriangles) {
    triangles.push_back(tri);
    bounds.extend(tri.vertices[0].p);
    bounds.extend(tri.vertices[1].p);
    bounds.extend(tri.vertices[2].p);
  }
  materials = meshMaterials;
}

void Scene::init(const std::vector<Triangle>& meshTriangles, const std::vector<BSDF*>& meshMaterials, Light* background) {
  init(meshTriangles, meshMaterials);
  this->background = background;
  if (background->totalIntegratedPower() > 0) lights.push_back(background);
}

void Scene::build() {
  bvh.build(triangles.data(), (int)triangles.size());
  for (int i = 0; i < (int)triangles.size(); i++) {
    if (materials[triangles[i].materialIndex]->isLight()) {
      AreaLight* light = new AreaLight();
      light->triangle = &triangles[i];
      light->emission = materials[triangles[i].materialIndex]->emission;
      lights.push_back(light);
    }
  }
}

IntersectionData Scene::traverse(const Ray& ray) {
  IntersectionData intersection;
  bvh.traverse(ray, intersection);
  return intersection;
}

Light* Scene::sampleLight(Sampler* sampler, float& pmf) {
  pmf = 1.0f / lights.size();
  int last  = (int)lights.size() - 1;
  int index = std::min((int)(sampler->next() * lights.size()), last);
  return lights[index];
}

Light* Scene::sampleLightWeighted(Sampler* sampler, float& pmf) {
  float totalPower = 0;
  for (auto* l : lights) totalPower += l->totalIntegratedPower();

  if (totalPower <= 0) return sampleLight(sampler, pmf);

  float sample = sampler->next() * totalPower;
  float curr = 0;
  for (auto* l : lights) {
    curr += l->totalIntegratedPower();
    if (sample <= curr) {
      pmf = l->totalIntegratedPower() / totalPower;
      return l;
    }
  }

  pmf = lights.back()->totalIntegratedPower() / totalPower;
  return lights.back();
}

bool Scene::visible(const Vec3& p1, const Vec3& p2) {
  Ray ray;
  Vec3 dir = p2 - p1;
  float maxT = dir.length() - (2.0f * RAY_EPSILON);
  dir = dir.normalize();
  ray.init(p1 + (dir * RAY_EPSILON), dir);
  return bvh.traverseVisible(ray, maxT);
}

Colour Scene::emit(Triangle* light, const ShadingData& shadingData, const Vec3& wi) {
  return materials[light->materialIndex]->emit(shadingData, wi);
}

float Scene::areaLightSelectionPDF(unsigned int triangleID) const {
  float totalPower = 0.0f;
  for (Light* light : lights) totalPower += light->totalIntegratedPower();
  if (totalPower <= 0.0f) return 0.0f;

  for (Light* light : lights) {
    if (!light->isArea()) continue;
    AreaLight* areaLight = static_cast<AreaLight*>(light);
    if (areaLight->triangle == &triangles[triangleID]) {
      return (areaLight->totalIntegratedPower() / totalPower) * (1.0f / areaLight->triangle->area);
    }
  }

  return 0.0f;
}

ShadingData Scene::calculateShadingData(IntersectionData intersection, const Ray& ray) {
  ShadingData shadingData = {};
  if (intersection.t < FLT_MAX) {
    shadingData.x = ray.at(intersection.t);
    shadingData.gNormal = triangles[intersection.ID].gNormal();
    triangles[intersection.ID].interpolateAttributes(intersection.alpha, intersection.beta, intersection.gamma, shadingData.sNormal, shadingData.tu, shadingData.tv);
    shadingData.bsdf = materials[triangles[intersection.ID].materialIndex];
    shadingData.wo = -ray.dir;
    if (shadingData.bsdf->isTwoSided()) {
      if (dot(shadingData.wo, shadingData.sNormal) < 0) shadingData.sNormal = -shadingData.sNormal;
      if (dot(shadingData.wo, shadingData.gNormal) < 0) shadingData.gNormal = -shadingData.gNormal;
    }
    shadingData.frame.fromVector(shadingData.sNormal);
    shadingData.t = intersection.t;
    return shadingData;
  }
  shadingData.wo = -ray.dir;
  shadingData.t = intersection.t;
  return shadingData;
}
