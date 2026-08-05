#include <filesystem>
#include <print>
#include <map>
#include <string>
#include "engine.h"
#include "config.h"

static const std::map<std::string, Config::IntegratorType> INTEGRATORS = {
  {"pt",     Config::IntegratorType::PathTracer},
  {"ir",     Config::IntegratorType::InstantRadiosity},
  {"restir", Config::IntegratorType::InstantRadiosityReSTIR},
};

static std::string integratorKeys() {
  std::string keys;
  for (const auto& entry : INTEGRATORS) {
    if (!keys.empty()) keys += "|";
    keys += entry.first;
  }
  return keys;
}

int main(int argc, char* argv[]) {
  if (argc < 3) {
    std::println("Usage: {} <scene-name> <{}> [--spp N] [--no-denoise]", argv[0], integratorKeys());
    std::println("  e.g. {} kitchen restir --spp 64", argv[0]);
    std::println("  e.g. {} bedroom pt --spp 4096   (reference build)", argv[0]);
    return 1;
  }

  std::string sceneName = argv[1];
  std::string integratorName = argv[2];

  std::string scenePath = std::string(Config::SCENE_PATH_PREFIX) + "/" + sceneName;

  auto integrator = INTEGRATORS.find(integratorName);
  if (integrator == INTEGRATORS.end()) {
    std::println("Error: unknown integrator '{}' (valid: {})", integratorName, integratorKeys());
    return 1;
  }

  Config::SCENE_NAME = sceneName;
  Config::INTEGRATOR_NAME = integrator->first;
  Config::INTEGRATOR = integrator->second;
  Config::OUTPUT_PREFIX = sceneName + "_" + Config::INTEGRATOR_NAME;

  for (int i = 3; i < argc; i++) {
    std::string arg = argv[i];
    if (arg == "--no-denoise") Config::USE_DENOISER = false;
    if (arg == "--spp" && i + 1 < argc) Config::TARGET_SPP = std::stoi(argv[++i]);
  }

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
