#include "material_loader.h"
#include <print>
#include <functional>
#include <unordered_map>

MaterialLoader::MaterialLoader(const std::string& sceneName) : sceneName(sceneName) {}

Texture* MaterialLoader::loadTexture(const std::string& relativePath) {
  std::string fullPath = sceneName + "/" + relativePath;
  auto it = cache.find(fullPath);
  if (it != cache.end()) return it->second;
  Texture* t = new Texture();
  t->load(fullPath);
  cache[fullPath] = t;
  return t;
}

BSDF* MaterialLoader::load(GEMLoader::GEMInstance& instance) {
  using Factory = std::function<BSDF*(GEMLoader::GEMInstance&, MaterialLoader&)>;
  static const std::unordered_map<std::string, Factory> factories = {

    {"diffuse", [](GEMLoader::GEMInstance& inst, MaterialLoader& ml) -> BSDF* {
      return new DiffuseBSDF(ml.loadTexture(inst.material.find("reflectance").getValue("")));
    }},

    {"mirror", [](GEMLoader::GEMInstance& inst, MaterialLoader& ml) -> BSDF* {
      Colour eta(0.0f, 0.0f, 0.0f);
      Colour k(0.0f, 0.0f, 0.0f);
      inst.material.find("eta").getValuesAsVector3(eta.r, eta.g, eta.b);
      inst.material.find("k").getValuesAsVector3(k.r, k.g, k.b);
      // Default to silver if not specified in the scene file
      if (eta.lum() == 0.0f && k.lum() == 0.0f) {
        eta = Colour(0.177f, 0.178f, 0.172f);
        k   = Colour(3.638f, 2.973f, 2.430f);
      }
      return new MirrorBSDF(ml.loadTexture(inst.material.find("reflectance").getValue("")), eta, k);
    }},


    {"glass", [](GEMLoader::GEMInstance& inst, MaterialLoader& ml) -> BSDF* {
      return new GlassBSDF(
        ml.loadTexture(inst.material.find("reflectance").getValue("")),
        inst.material.find("intIOR").getValue(1.5f),
        inst.material.find("extIOR").getValue(1.0f)
      );
    }},

    {"plastic", [](GEMLoader::GEMInstance& inst, MaterialLoader& ml) -> BSDF* {
      return new PlasticBSDF(
        ml.loadTexture(inst.material.find("reflectance").getValue("")),
        inst.material.find("intIOR").getValue(1.5f),
        inst.material.find("extIOR").getValue(1.0f),
        inst.material.find("roughness").getValue(0.5f)
      );
    }},

    {"conductor", [](GEMLoader::GEMInstance& inst, MaterialLoader& ml) -> BSDF* {
      Colour eta(0.0f, 0.0f, 0.0f);
      Colour k(0.0f, 0.0f, 0.0f);
      inst.material.find("eta").getValuesAsVector3(eta.r, eta.g, eta.b);
      inst.material.find("k").getValuesAsVector3(k.r, k.g, k.b);
      return new ConductorBSDF(
        ml.loadTexture(inst.material.find("reflectance").getValue("")),
        eta, k,
        inst.material.find("roughness").getValue(0.5f)
      );
    }},
  
  };

  std::string type = instance.material.find("bsdf").getValue("");
  auto it = factories.find(type);
  if (it == factories.end()) {
    std::println("material '{}' not supported, skipping instance", type);
    return nullptr;
  }

  BSDF* bsdf = it->second(instance, *this);

  if (instance.material.find("emission").getValue("") != "") {
    Colour emission(0.0f, 0.0f, 0.0f);
    instance.material.find("emission").getValuesAsVector3(emission.r, emission.g, emission.b);
    bsdf->addLight(emission);
  }

  return bsdf;
}
