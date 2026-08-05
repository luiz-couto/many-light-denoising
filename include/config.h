#pragma once

namespace Config {
  enum class IntegratorType { PathTracer, InstantRadiosity, InstantRadiosityReSTIR };

  constexpr IntegratorType INTEGRATOR = IntegratorType::PathTracer;
  constexpr int TILE_SIZE = 64;
  constexpr const char* TITLE = "many-light-denoising";
  constexpr const char* OUTPUT_PATH = "output.png";
  constexpr const char* SCENE_PATH_PREFIX = "scenes";
  inline bool USE_DENOISER = true;

  // Path Tracer
  constexpr int PT_MAX_DEPTH = 20;
  constexpr int PT_RR_DEPTH  = 3;

  // IR baseline (compile-time)
  constexpr int IR_NUM_LIGHT_PATHS = 4096 * 2;
  constexpr int IR_MAX_PHOTON_DEPTH = 100;
  constexpr int IR_MAX_SPECULAR_DEPTH = 8;
  constexpr float IR_G_CLAMP = 10.0f;
  constexpr bool  IR_DECOUPLED_SHADING = true;
  constexpr float IR_FOOTPRINT_FRACTION = 0.02f;
  constexpr float IR_GLOSSY_WALK_ALPHA = 0.04f;

  // ReSTIR IR
  constexpr int IR_RESTIR_SPATIAL_ROUNDS = 1;
  constexpr int IR_RESTIR_M = 32;
  constexpr int IR_RESTIR_K = 5;
  constexpr int IR_RESTIR_RADIUS = 10;
  constexpr float IR_RESTIR_NORMAL_THRESHOLD = 0.9f; // ~25 degrees
  constexpr float IR_RESTIR_DEPTH_THRESHOLD  = 0.1f; // 10% relative hit-distance gap
}
