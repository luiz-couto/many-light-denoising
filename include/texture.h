#ifndef TEXTURE_H
#define TEXTURE_H

#include "core.h"
#include <string>

class Texture {
public:
  Colour* texels;
	float* alpha;
	int width;
	int height;
	int channels;

  Texture();
  ~Texture();

  void loadDefault();
  void load(std::string filename);
  void loadHDR(std::string filename);
  Colour sample(const float tu, const float tv) const;
  float sampleAlpha(const float tu, const float tv) const;
};

#endif // TEXTURE_H
