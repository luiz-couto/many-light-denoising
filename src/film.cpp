#include "film.h"
#include "config.h"
#include <OpenImageDenoise/oidn.hpp>
#include <vector>
#include <algorithm>

Film::Film() {}

// Allocates the pixel buffer to width * height entries and resets the film via clear().
void Film::init(int width, int height) {
  this->width = width;
  this->height = height;

  film.resize(width * height);
  filmNormals.resize(width * height);
  filmAlbedos.resize(width * height);
  filmDenoised.resize(width * height);

  clear();
}

// Accumulates radiance L at pixel (x, y). Truncates to integer pixel coordinates
// and silently discards samples outside the film bounds.
void Film::splat(const float x, const float y, const Colour& L) {
  int px = (int)x;
  int py = (int)y;

  if (px < 0 || px >= (int)width || py < 0 || py >= (int)height) return;

  film[py * width + px] = film[py * width + px] + L;
}

// Zeros all accumulated samples and resets SPP to 0.
void Film::clear() {
  std::fill(film.begin(), film.end(), Colour(0.0f, 0.0f, 0.0f));
  SPP = 0;
}

// Increments the sample-per-pixel counter. Call once per completed full-image pass.
void Film::incrementSPP() {
  SPP++;
}

// Tonemaps every pixel and returns a flat width*height*3 uint8_t RGB buffer
// suitable for passing directly to Window::savePNG.
std::vector<uint8_t> Film::toPixels() {
  std::vector<uint8_t> pixels(width * height * 3);

  for (int y = 0; y < (int)height; y++) {
    for (int x = 0; x < (int)width; x++) {
      unsigned char r, g, b;
      tonemap(x, y, r, g, b);
      int idx = (y * width + x) * 3;

      pixels[idx]     = r;
      pixels[idx + 1] = g;
      pixels[idx + 2] = b;
    }
  }

  return pixels;
}

// Filmic curve helper. Maps a linear HDR value to a
// compressed range before the final white-point normalisation in tonemap().
float Film::filmicCFunc(float value) {
  float A = 0.15f;
  float B = 0.50f;
  float C = 0.10f;
  float D = 0.20f;
  float E = 0.02f;
  float F = 0.30f;

  float v1 = (value * (A * value + C * B) + D * E);
  float v2 = (value * (A * value + B) + D * F);
  float v3 = E / F;

  return (v1 / v2) - v3;
}

// Converts the denoised pixel at (x, y) to gamma-corrected uint8_t RGB.
// Reads from filmDenoised (already SPP-normalized by denoise()), applies
// exposure, runs the filmic curve, gamma (1/2.2), and clamps to [0, 255].
void Film::tonemap(int x, int y, unsigned char& r, unsigned char& g, unsigned char& b, float exposure) {
  Colour curr = Config::USE_DENOISER
    ? filmDenoised[(y * width) + x] * exposure
    : (SPP > 0 ? film[(y * width) + x] / (float)SPP : Colour(0.0f, 0.0f, 0.0f)) * exposure;

  float expFac = 1.0f / 2.2f;
  float W = 11.2f;
  float CW = filmicCFunc(W);
  float CR = filmicCFunc(curr.r);
  float rOut = powf((CR / CW), expFac);

  float CG = filmicCFunc(curr.g);
  float gOut = powf((CG / CW), expFac);

  float CB = filmicCFunc(curr.b);
  float bOut = powf((CB / CW), expFac);

  r = std::clamp(rOut, 0.0f, 1.0f) * 255;
  g = std::clamp(gOut, 0.0f, 1.0f) * 255;
  b = std::clamp(bOut, 0.0f, 1.0f) * 255;
}

void Film::setNormal(int x, int y, const Colour& L) {
  if (x < 0 || x >= width || y < 0 || y >= height) return;
  filmNormals[(y * width) + x] = L;
}

void Film::setAlbedo(int x, int y, const Colour& albedo) {
  if (x < 0 || x >= width || y < 0 || y >= height) return;
  filmAlbedos[(y * width) + x] = albedo;
}

void Film::denoise() {
  std::vector<Colour> filmNormalized(width * height);
  float invSPP = SPP > 0 ? 1.0f / (float)SPP : 1.0f;
  for (int i = 0; i < (int)filmNormalized.size(); i++) {
    filmNormalized[i] = film[i] * invSPP;
  }

  oidn::DeviceRef device = oidn::newDevice();
  device.commit();
  oidn::FilterRef filter = device.newFilter("RT");
  filter.setImage("color", filmNormalized.data(), oidn::Format::Float3, width, height);
  filter.setImage("albedo", filmAlbedos.data(), oidn::Format::Float3, width, height);
  filter.setImage("normal", filmNormals.data(), oidn::Format::Float3, width, height);
  filter.setImage("output", filmDenoised.data(), oidn::Format::Float3, width, height);
  filter.set("hdr", true);
  filter.commit();
  filter.execute();
}
