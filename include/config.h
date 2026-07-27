#pragma once

namespace Config {
  enum class IntegratorType { PathTracer };

  constexpr IntegratorType INTEGRATOR = IntegratorType::PathTracer;
  constexpr int TILE_SIZE = 64;
  constexpr const char* TITLE = "many-light-denoising";
  constexpr const char* OUTPUT_PATH = "output.png";
  constexpr const char* SCENE_PATH = "scenes/kitchen";
}
