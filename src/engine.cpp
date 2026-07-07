#include "engine.h"

Engine::Engine(): 
  window(Config::TITLE, Config::WIDTH, Config::HEIGHT),
  frameBuffer(Config::WIDTH * Config::HEIGHT * 3, 0) {}

void Engine::run() {
  render();
  while(window.pollEvents()) {
    window.update(frameBuffer.data());
  }
}

void Engine::render() {
  for(int y = 0; y < Config::HEIGHT; ++y) {
    for(int x = 0; x < Config::WIDTH; ++x) {
      int index = (y * Config::WIDTH + x) * 3;
      frameBuffer[index] = static_cast<uint8_t>((x / static_cast<float>(Config::WIDTH)) * 255); // Red
      frameBuffer[index + 1] = static_cast<uint8_t>((y / static_cast<float>(Config::HEIGHT)) * 255); // Green
      frameBuffer[index + 2] = 128; // Blue
    }
  }
}
