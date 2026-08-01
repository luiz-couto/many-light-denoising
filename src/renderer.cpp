#include "renderer.h"
#include "path_tracer.h"
#include "instant_radiosity.h"
#include <functional>
#include <unordered_map>

Renderer::Renderer() :
  integrator(std::make_unique<PathTracerIntegrator>(&scene, &film)) {}

Renderer::Renderer(const std::string& scenePath, Config::IntegratorType type) {
  scene.load(scenePath);
  film.init(scene.width, scene.height);

  using Factory = std::function<std::unique_ptr<Integrator>(Scene*, Film*)>;
  static const std::unordered_map<Config::IntegratorType, Factory> factories = {
    { Config::IntegratorType::PathTracer,
      [](Scene* s, Film* f) { return std::make_unique<PathTracerIntegrator>(s, f); } },
    { Config::IntegratorType::InstantRadiosity,
      [](Scene* s, Film* f) { return std::make_unique<InstantRadiosityIntegrator>(s, f); } },
  };

  integrator = factories.at(type)(&scene, &film);
}

void Renderer::render() {
  integrator->render();
}
