#ifndef MATERIAL_LOADER_H
#define MATERIAL_LOADER_H

#include <map>
#include <string>
#include "GEMLoader.h"
#include "shading.h"
#include "texture.h"

class MaterialLoader {
  std::string sceneName;
  std::map<std::string, Texture*> cache;

public:
  explicit MaterialLoader(const std::string& sceneName);
  Texture* loadTexture(const std::string& relativePath);
  BSDF* load(GEMLoader::GEMInstance& instance);
};

#endif // MATERIAL_LOADER_H
