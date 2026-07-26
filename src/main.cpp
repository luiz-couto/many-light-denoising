#include <print>
#include "engine.h"
#include "config.h"

int main() {
  try {
    Engine engine(Config::SCENE_PATH);
    engine.run();
  } catch (const std::exception& err) {
    std::println("Fatal error: {}", err.what());
    return 1;
  }
  return 0;
}
