#ifndef FILM_H
#define FILM_H

#include "core.h"
#include <vector>

class Film {
public:
	std::vector<Colour> film;
  std::vector<Colour> filmNormals;   // first-hit shading normals
  std::vector<Colour> filmAlbedos;   // first-hit albedo
  std::vector<Colour> filmDenoised;  // OIDN output

	unsigned int width;
	unsigned int height;
	int SPP = 0;

  Film();
	void init(int width, int height);
	void splat(const float x, const float y, const Colour& L);
	void clear();
	void incrementSPP();
	void tonemap(int x, int y, unsigned char& r, unsigned char& g, unsigned char& b, float exposure = 1.0f);
	void tonemapValue(const Colour& value, unsigned char& r, unsigned char& g, unsigned char& b);
	float filmicCFunc(float value);

  void setNormal(int x, int y, const Colour& L);
  void setAlbedo(int x, int y, const Colour& albedo);
  void denoise();
  std::vector<uint8_t> toPixels();
  // Tonemaps a caller-supplied linear HDR buffer (already SPP-normalized),
  // ignoring the denoiser flag — used to save raw and denoised PNGs.
  std::vector<uint8_t> toPixels(const std::vector<Colour>& buffer);
};

#endif // FILM_H
