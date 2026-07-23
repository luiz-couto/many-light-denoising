#ifndef FILM_H
#define FILM_H

#include "core.h"
#include <vector>

class Film {
public:
	std::vector<Colour> film;
	// TODO (denoiser step): filmNormals, filmAlbedos, output

	unsigned int width;
	unsigned int height;
	int SPP = 0;

  Film();
	void init(int width, int height);
	void splat(const float x, const float y, const Colour& L);
	void clear();
	void incrementSPP();
	void tonemap(int x, int y, unsigned char& r, unsigned char& g, unsigned char& b, float exposure = 1.0f);
	float filmicCFunc(float value);
	std::vector<uint8_t> toPixels();
};

#endif // FILM_H
