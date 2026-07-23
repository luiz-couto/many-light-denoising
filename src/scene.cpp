#include "scene.h"
#include <algorithm>

Scene::Scene() {}

void Scene::init(const std::vector<Triangle>& meshTriangles, const std::vector<BSDF*>& meshMaterials, Light* background) {
  // read background light
  this->background = background;
  if (background->totalIntegratedPower() > 0) {
		lights.push_back(background);
	}
  
  // read triangles
  for (int i = 0; i < meshTriangles.size(); i++) {
    triangles.push_back(meshTriangles[i]);
    bounds.extend(meshTriangles[i].vertices[0].p);
    bounds.extend(meshTriangles[i].vertices[1].p);
    bounds.extend(meshTriangles[i].vertices[2].p);
	}

  materials = meshMaterials;
}

void Scene::build() {
  bvh.build(triangles.data(), (int)triangles.size());

  // TODO: walk triangles, promote emissive materials to AreaLight and push to lights
  // for (int i = 0; i < triangles.size(); i++) {
  //   if (materials[triangles[i].materialIndex]->isLight()) {
  //     AreaLight* light = new AreaLight();
  //     light->triangle = &triangles[i];
  //     light->emission = materials[triangles[i].materialIndex]->emission;
  //     lights.push_back(light);
  //   }
  // }
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
  for (int i=0; i<lights.size(); i++) {
    totalPower += lights[i]->totalIntegratedPower();
  }

  if (totalPower <= 0) return sampleLight(sampler, pmf);

  float sample = sampler->next() * totalPower;
  float curr = 0;

  for (int i=0; i<lights.size(); i++) {
    curr += lights[i]->totalIntegratedPower();
    if (sample <= curr) {
      pmf = lights[i]->totalIntegratedPower() / totalPower;
      return lights[i];
    }
  }

  // Fallback: floating-point drift, return last light
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
