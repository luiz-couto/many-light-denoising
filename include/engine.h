#ifndef ENGINE_H
#define ENGINE_H

#include "window.h"
#include "config.h"
#include <vector>

class Engine {
  public:
    Engine();

    // Main loop
    void run();

  private:
    Window window;
    std::vector<uint8_t> frameBuffer;

    // Rendering function
    void render();
};

#endif // ENGINE_H
