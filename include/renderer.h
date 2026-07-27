#ifndef RENDERER_H
#define RENDERER_H

#include <memory>
#include <string>
#include "integrator.h"
#include "config.h"

class Renderer {
public:
  Scene scene;
  Film  film;
  std::unique_ptr<Integrator> integrator;

  Renderer();
  Renderer(const std::string& scenePath, Config::IntegratorType type);
  void render();
};

#endif // RENDERER_H
