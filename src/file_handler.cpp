#include "file_handler.h"
#include "config.h"
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

static std::string timestamp() {
  std::time_t t = std::time(nullptr);
  char buffer[32];
  std::strftime(buffer, sizeof(buffer), "%Y%m%d_%H%M%S", std::localtime(&t));
  return std::string(buffer);
}

void FileHandler::saveOutputs(Film& film, double renderSeconds) {
  std::filesystem::create_directories(Config::OUTPUTS_FOLDER);
  std::string currentTimestamp = timestamp();
  std::string base =
    "output/" +
    Config::OUTPUT_PREFIX +
    "_spp" +
    std::to_string(film.SPP) +
    "_" + currentTimestamp;

  std::vector<Colour> averaged = film.film;
  float invSPP = 1.0f / (float)film.SPP;
  for (Colour& c : averaged) c = c * invSPP;

  // Raw always: plain name. Denoised: _denoised suffix (mirrors the pfm pair).
  writePFM(base + ".pfm", averaged, film.width, film.height);
  savePNG(base + ".png", film.toPixels(averaged), film.width, film.height);

  if (Config::USE_DENOISER) {
    writePFM(base + "_denoised.pfm", film.filmDenoised, film.width, film.height);
    savePNG(base + "_denoised.png", film.toPixels(film.filmDenoised), film.width, film.height);
  }

  writeJSONMetadata(film, base + ".json", renderSeconds, currentTimestamp);
}

void FileHandler::writePFM(const std::string& path, const std::vector<Colour>& buffer, unsigned int width, unsigned int height) {
  std::ofstream file(path, std::ios::binary);
  if (!file.is_open()) {
    std::cerr << "writePFM: cannot open " << path << std::endl;
    return;
  }

  // Header: "PF" = 3-channel; negative scale = little-endian floats.
  file << "PF\n" << width << " " << height << "\n-1.0\n";

  // Body: stores the bottom row first
  for (int y = (int)height - 1; y >= 0; y--) {
    file.write(
      reinterpret_cast<const char*>(&buffer[(size_t)y * width]),
      (std::streamsize)(width * sizeof(Colour))
    );
  }
}

void FileHandler::savePNG(const std::string& path, const std::vector<uint8_t>& pixels, unsigned int width, unsigned int height) {
  stbi_write_png(
    path.c_str(),
    (int)width,
    (int)height,
    3,
    pixels.data(),
    (int)width * 3
  );
}

void FileHandler::writeJSONMetadata(Film& film, const std::string& path, double renderSeconds, const std::string& timestampString) {
  std::ofstream file(path);
  if (!file.is_open()) {
    std::cerr << "writeJSONMetadata: cannot open " << path << std::endl;
    return;
  }

  file << std::boolalpha;
  file << "{\n";
  file << "  \"scene\": \"" << Config::SCENE_NAME << "\",\n";
  file << "  \"integrator\": \"" << Config::INTEGRATOR_NAME << "\",\n";
  file << "  \"timestamp\": \"" << timestampString << "\",\n";
  file << "  \"width\": " << film.width << ",\n";
  file << "  \"height\": " << film.height << ",\n";
  file << "  \"spp\": " << film.SPP << ",\n";
  file << "  \"render_seconds\": " << renderSeconds << ",\n";
  file << "  \"use_denoiser\": " << Config::USE_DENOISER << ",\n";
  file << "  \"pt\": {\n";
  file << "    \"max_depth\": " << Config::PT_MAX_DEPTH << ",\n";
  file << "    \"rr_depth\": " << Config::PT_RR_DEPTH << "\n";
  file << "  },\n";
  file << "  \"ir\": {\n";
  file << "    \"num_light_paths\": " << Config::IR_NUM_LIGHT_PATHS << ",\n";
  file << "    \"max_photon_depth\": " << Config::IR_MAX_PHOTON_DEPTH << ",\n";
  file << "    \"max_specular_depth\": " << Config::IR_MAX_SPECULAR_DEPTH << ",\n";
  file << "    \"g_clamp\": " << Config::IR_G_CLAMP << ",\n";
  file << "    \"decoupled_shading\": " << Config::IR_DECOUPLED_SHADING << ",\n";
  file << "    \"footprint_fraction\": " << Config::IR_FOOTPRINT_FRACTION << ",\n";
  file << "    \"glossy_walk_alpha\": " << Config::IR_GLOSSY_WALK_ALPHA << "\n";
  file << "  },\n";
  file << "  \"restir\": {\n";
  file << "    \"spatial_rounds\": " << Config::IR_RESTIR_SPATIAL_ROUNDS << ",\n";
  file << "    \"m\": " << Config::IR_RESTIR_M << ",\n";
  file << "    \"k\": " << Config::IR_RESTIR_K << ",\n";
  file << "    \"radius\": " << Config::IR_RESTIR_RADIUS << ",\n";
  file << "    \"normal_threshold\": " << Config::IR_RESTIR_NORMAL_THRESHOLD << ",\n";
  file << "    \"depth_threshold\": " << Config::IR_RESTIR_DEPTH_THRESHOLD << "\n";
  file << "  }\n";
  file << "}\n";
}
