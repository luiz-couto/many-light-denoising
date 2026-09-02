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
    std::println("Usage: {} <scene-name> <{}> [--spp N] [--no-denoise] [--snapshot N|log] [--ir-paths N] [--ir-depth N] [--ir-jitter on|off] [--restir-rounds N]", argv[0], integratorKeys());
    std::println("  e.g. {} kitchen restir --spp 64", argv[0]);
    std::println("  e.g. {} bedroom pt --spp 4096   (reference build)", argv[0]);
    std::println("  e.g. {} bathroom restir --spp 256 --snapshot log   (artifacts at spp 1,2,4,...,256)", argv[0]);
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
    if (arg == "--snapshot" && i + 1 < argc) {
      std::string value = argv[++i];
      if (value == "log") Config::SNAPSHOT_LOG = true;
      else Config::SNAPSHOT_INTERVAL = std::stoi(value);
    }
    if (arg == "--ir-paths" && i + 1 < argc) Config::IR_NUM_LIGHT_PATHS = std::stoi(argv[++i]);
    if (arg == "--ir-depth" && i + 1 < argc) Config::IR_MAX_PHOTON_DEPTH = std::stoi(argv[++i]);
    if (arg == "--ir-jitter" && i + 1 < argc) Config::IR_DECOUPLED_SHADING = std::string(argv[++i]) == "on";
    if (arg == "--restir-rounds" && i + 1 < argc) Config::IR_RESTIR_SPATIAL_ROUNDS = std::stoi(argv[++i]);
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
