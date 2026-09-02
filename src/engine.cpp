#include "engine.h"
#include "config.h"
#include "file_handler.h"
#include <chrono>

Engine::Engine(const std::string& scenePath):
  renderer(scenePath, Config::INTEGRATOR),
  window(Config::TITLE, renderer.scene.width, renderer.scene.height),
  frameBuffer(renderer.scene.width * renderer.scene.height * 3, 0) {}

void Engine::run() {
  bool running = true;
  double renderSeconds = 0.0;

  while (running) {
    auto passStart = std::chrono::steady_clock::now();

    renderer.render();
    if (Config::USE_DENOISER) renderer.film.denoise();

    renderSeconds += std::chrono::duration<double>(std::chrono::steady_clock::now() - passStart).count();

    frameBuffer = renderer.film.toPixels();

    int spp = renderer.film.SPP;
    bool finalPass = Config::TARGET_SPP > 0 && spp >= Config::TARGET_SPP;
    bool snapshotDue =
      (Config::SNAPSHOT_INTERVAL > 0 && spp % Config::SNAPSHOT_INTERVAL == 0) ||
      (Config::SNAPSHOT_LOG && (spp & (spp - 1)) == 0);   // power of two

    if (snapshotDue && !finalPass) {
      FileHandler::saveOutputs(renderer.film, renderSeconds);
    }

    if (finalPass) {
      FileHandler::saveOutputs(renderer.film, renderSeconds);
      running = false;
    }

    switch (window.pollEvents()) {
      case Window::Event::Quit:
        running = false;
        break;
      case Window::Event::SaveImage:
        FileHandler::saveOutputs(renderer.film, renderSeconds);
        break;
      default:
        break;
    }
    window.update(frameBuffer.data());
  }
}
