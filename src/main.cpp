#include <print>
#include "engine.h"

int main() {
  try {
    Engine engine;
    engine.run();
  } catch (const std::exception& err) {
    std::println("Fatal error: {}", err.what());
    return 1;
  }
  return 0;
}
