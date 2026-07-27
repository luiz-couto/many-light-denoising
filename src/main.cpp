#include <filesystem>
#include <print>
#include "engine.h"
#include "config.h"

int main(int argc, char* argv[]) {
  if (argc < 2) {
    std::println("Usage: {} <scene-name>", argv[0]);
    std::println("  e.g. {} kitchen", argv[0]);
    return 1;
  }

  std::string scenePath = std::string(Config::SCENE_PATH_PREFIX) + "/" + argv[1];

  if (!std::filesystem::is_directory(scenePath)) {
    std::println("Error: scene folder '{}' does not exist", scenePath);
    return 1;
  }

  try {
    Engine engine(scenePath);
    engine.run();
  } catch (const std::exception& err) {
    std::println("Fatal error: {}", err.what());
    return 1;
  }
  return 0;
}
