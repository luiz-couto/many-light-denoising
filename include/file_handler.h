#ifndef FILE_HANDLER_H
#define FILE_HANDLER_H

#include <string>
#include <vector>
#include "film.h"

// All disk output lives here: pfm, png, json metadata.
class FileHandler {
public:
  // Writes the per-run artifact set (pfm, denoised pfm, png, json metadata)
  // under Config::OUTPUTS_FOLDER with one shared timestamped basename.
  static void saveOutputs(Film& film, const std::vector<uint8_t>& frameBuffer, double renderSeconds);

  // Writes a linear HDR buffer as PFM (little-endian, bottom row first).
  static void writePFM(const std::string& path, const std::vector<Colour>& buffer, unsigned int width, unsigned int height);

  // Writes an 8-bit RGB buffer as PNG.
  static void savePNG(const std::string& path, const std::vector<uint8_t>& pixels, unsigned int width, unsigned int height);
  static void writeJSONMetadata(Film& film, const std::string& path, double renderSeconds, const std::string& timestampString);
};

#endif // FILE_HANDLER_H
