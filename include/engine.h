#ifndef ENGINE_H
#define ENGINE_H

#include <string>
#include <vector>
#include "window.h"
#include "renderer.h"

class Engine {
  public:
    Engine(const std::string& scenePath);

    // Main loop
    void run();

    Renderer renderer;

  private:
    Window window;
    std::vector<uint8_t> frameBuffer;
};

#endif // ENGINE_H
