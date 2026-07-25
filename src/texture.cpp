#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "texture.h"

constexpr const char* HDR_EXTENSION = ".hdr";

Texture::Texture() : texels(nullptr), alpha(nullptr), width(0), height(0), channels(0) {}

Texture::~Texture() {
  delete[] texels;
  if (alpha != nullptr) delete[] alpha;
}

void Texture::loadDefault() {
  width = 1;
  height = 1;
  channels = 3;
  texels = new Colour[1];
  texels[0] = Colour(1.0f, 1.0f, 1.0f);
}

void Texture::load(std::string filename) {
  alpha = NULL;
	if (filename.find(HDR_EXTENSION) != std::string::npos) {
    return loadHDR(filename);
  }

  unsigned char* textureData = stbi_load(filename.c_str(), &width, &height, &channels, 0);
	if (width == 0 || height == 0) return loadDefault();

  texels = new Colour[width * height];
  for (int i = 0; i < (width * height); i++) {
    texels[i] = Colour(textureData[i * channels] / 255.0f, textureData[(i * channels) + 1] / 255.0f, textureData[(i * channels) + 2] / 255.0f);
  }

  if (channels == 4) {
    alpha = new float[width * height];
    for (int i = 0; i < (width * height); i++) {
      alpha[i] = textureData[(i * channels) + 3] / 255.0f;
    }
  }

  stbi_image_free(textureData);
}

void Texture::loadHDR(std::string filename) {
  float* textureData = stbi_loadf(filename.c_str(), &width, &height, &channels, 0);
  if (width == 0 || height == 0) return loadDefault();

  texels = new Colour[width * height];
  for (int i = 0; i < (width * height); i++) {
    texels[i] = Colour(textureData[i * channels], textureData[(i * channels) + 1], textureData[(i * channels) + 2]);
  }

  stbi_image_free(textureData);
}

Colour Texture::sample(const float tu, const float tv) const {
  Colour tex;

  float u = std::max(0.0f, fabsf(tu)) * width;
  float v = std::max(0.0f, fabsf(tv)) * height;
  int x = (int)floorf(u);
  int y = (int)floorf(v);

  float frac_u = u - x;
  float frac_v = v - y;
  float w0 = (1.0f - frac_u) * (1.0f - frac_v);
  float w1 = frac_u * (1.0f - frac_v);
  float w2 = (1.0f - frac_u) * frac_v;
  float w3 = frac_u * frac_v;

  x = x % width;
  y = y % height;

  Colour s[4];
  s[0] = texels[y * width + x];
  s[1] = texels[y * width + ((x + 1) % width)];
  s[2] = texels[((y + 1) % height) * width + x];
  s[3] = texels[((y + 1) % height) * width + ((x + 1) % width)];

  tex = (s[0] * w0) + (s[1] * w1) + (s[2] * w2) + (s[3] * w3);
  return tex;
}

float Texture::sampleAlpha(const float tu, const float tv) const {
  if (alpha == NULL) return 1.0f;

  float tex;
  float u = std::max(0.0f, fabsf(tu)) * width;
  float v = std::max(0.0f, fabsf(tv)) * height;
  int x = (int)floorf(u);
  int y = (int)floorf(v);

  float frac_u = u - x;
  float frac_v = v - y;
  float w0 = (1.0f - frac_u) * (1.0f - frac_v);
  float w1 = frac_u * (1.0f - frac_v);
  float w2 = (1.0f - frac_u) * frac_v;
  float w3 = frac_u * frac_v;

  x = x % width;
  y = y % height;

  float s[4];
  s[0] = alpha[y * width + x];
  s[1] = alpha[y * width + ((x + 1) % width)];
  s[2] = alpha[((y + 1) % height) * width + x];
  s[3] = alpha[((y + 1) % height) * width + ((x + 1) % width)];

  tex = (s[0] * w0) + (s[1] * w1) + (s[2] * w2) + (s[3] * w3);
  return tex;
}
