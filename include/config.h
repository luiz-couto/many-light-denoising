#pragma once

namespace Config {
  enum class IntegratorType { PathTracer, InstantRadiosity };

  constexpr IntegratorType INTEGRATOR = IntegratorType::InstantRadiosity;
  constexpr int TILE_SIZE = 64;
  constexpr const char* TITLE = "many-light-denoising";
  constexpr const char* OUTPUT_PATH = "output.png";
  constexpr const char* SCENE_PATH_PREFIX = "scenes";
  inline bool USE_DENOISER = true;

  // IR baseline (compile-time)
  constexpr int IR_NUM_LIGHT_PATHS = 64;
  constexpr int IR_MAX_PHOTON_DEPTH = 4;
  constexpr int IR_MAX_SPECULAR_DEPTH = 5;
  constexpr float IR_G_CLAMP = 10.0f;
}
