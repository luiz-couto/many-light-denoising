#include "engine.h"

Engine::Engine():
  window(Config::TITLE, Config::WIDTH, Config::HEIGHT),
  frameBuffer(Config::WIDTH * Config::HEIGHT * 3, 0) {}

void Engine::run() {
  renderer.render();
  frameBuffer = renderer.film.toPixels();

  bool running = true;
  while (running) {
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
