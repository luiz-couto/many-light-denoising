#ifndef RENDERER_H
#define RENDERER_H

#include <atomic>
#include "config.h"
#include "scene.h"
#include "film.h"

class Renderer {
public:
  Scene scene;
  Film film;

  Renderer();
  void renderTile(int threadId, std::atomic<unsigned int>& tileId, MTRandom& sampler);
  void render();
};

#endif // RENDERER_H
