#include "engine.h"
#include "config.h"

Engine::Engine(const std::string& scenePath):
  renderer(scenePath, Config::INTEGRATOR),
  window(Config::TITLE, renderer.scene.width, renderer.scene.height),
  frameBuffer(renderer.scene.width * renderer.scene.height * 3, 0) {}

void Engine::run() {
  bool running = true;
  while (running) {
    renderer.render();
    frameBuffer = renderer.film.toPixels();

    switch (window.pollEvents()) {
      case Window::Event::Quit:
        running = false;
        break;
      case Window::Event::SaveImage:
        window.savePNG(Config::OUTPUT_PATH, frameBuffer.data());
        break;
      default:
        break;
    }
    window.update(frameBuffer.data());
  }
}
