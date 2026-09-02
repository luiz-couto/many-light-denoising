#include "light.h"
#include <algorithm>

// Default stub. only needed for bidirectional methods.
// Lights that don't support it inherit this and return pdf=0 to signal "not available".
Vec3 Light::samplePositionFromLight(Sampler* sampler, float& pdf) {
	pdf = 0.0f;
	return Vec3(0.0f, 0.0f, 0.0f);
}

Vec3 AreaLight::sample(const ShadingData& shadingData, Sampler* sampler, Colour& emittedColour, float& pdf) {
  emittedColour = emission;
	return triangle->sample(sampler, pdf);
}

Colour AreaLight::evaluate(const Vec3& wi) {
  if (dot(wi, triangle->gNormal()) < 0) return emission;
  return Colour(0.0f, 0.0f, 0.0f);
}

// Returns the pdf in area measure (1/area), consistent with sample().
// The integrator must convert to solid-angle measure for MIS:
//   pdf_sa = pdf_area * dist² / dot(-wi, light_normal)
// where dist is the distance from the shading point to the sampled light position.
float AreaLight::PDF(const ShadingData& shadingData, const Vec3& wi) {
  return 1.0f / triangle->area;
}

bool AreaLight::isArea() {
  return true;
}

Vec3 AreaLight::normal(const ShadingData& shadingData, const Vec3& wi) {
  return triangle->gNormal();
}

float AreaLight::totalIntegratedPower() {
  return (triangle->area * emission.lum() * M_PI);
}

Vec3 AreaLight::samplePositionFromLight(Sampler* sampler, float& pdf) {
  return triangle->sample(sampler, pdf);
}

Vec3 AreaLight::sampleDirectionFromLight(Sampler* sampler, float& pdf) {
  Vec3 wi = SamplingDistributions::cosineSampleHemisphere(sampler->next(), sampler->next());
  pdf = SamplingDistributions::cosineHemispherePDF(wi);

  Frame frame;
  frame.fromVector(triangle->gNormal());
  return frame.toWorld(wi);
}

BackgroundColour::BackgroundColour(Colour _emission): emission(_emission) {}

Vec3 BackgroundColour::sample(const ShadingData& shadingData, Sampler* sampler, Colour& reflectedColour, float& pdf) {
  Vec3 wi = SamplingDistributions::uniformSampleSphere(sampler->next(), sampler->next());
  pdf = SamplingDistributions::uniformSpherePDF(wi);
  reflectedColour = emission;
  return wi;
}

Colour BackgroundColour::evaluate(const Vec3& wi) {
  return emission;
}

float BackgroundColour::PDF(const ShadingData& shadingData, const Vec3& wi) {
  return SamplingDistributions::uniformSpherePDF(wi);
}

bool BackgroundColour::isArea() {
  return false;
}

Vec3 BackgroundColour::normal(const ShadingData& shadingData, const Vec3& wi) {
  return -wi;
}

float BackgroundColour::totalIntegratedPower() {
  return emission.lum() * 4.0f * PI;
}

Vec3 BackgroundColour::sampleDirectionFromLight(Sampler* sampler, float& pdf) {
  Vec3 wi = SamplingDistributions::uniformSampleSphere(sampler->next(), sampler->next());
  pdf = SamplingDistributions::uniformSpherePDF(wi);
  return wi;
}

EnvironmentMap::EnvironmentMap(Texture* _env, Vec3 _sceneCentre, float _sceneRadius)
  : env(_env), sceneCentre(_sceneCentre), sceneRadius(_sceneRadius) {
  buildCDF();
  totalIntegratedPower();
}

EnvironmentMap::~EnvironmentMap() {
  for (int u = 0; u < env->height; u++) {
    delete[] rowCDF[u];
  }
  delete[] rowCDF;
  delete[] columnCDF;
}

int EnvironmentMap::cdfBinarySearch(float* list, int length, float s) {
  int left = 0;
  int right = length - 1;

  while (left < right) {
    int mid = (left + right) / 2;
    if (list[mid] >= s) {
      right = mid;
    } else {
      left = mid + 1;
    }
  }

  return left;
}

void EnvironmentMap::buildCDF() {
  int colorCounter = 0;
  rowCDF = new float*[env->height];

  float sum = 0;
  for (int u=0; u<env->height; u++) {
    rowCDF[u] = new float[env->width];
    for (int v=0; v<env->width; v++) {
      Colour color = env->texels[colorCounter];
      colorCounter++;

      float lum = color.lum();
      float fuv = lum * sinf(M_PI * (float)u / (float)env->height);
      sum += fuv;

      rowCDF[u][v] = fuv;
    }
  }

  this->totalSum = sum;
  columnCDF = new float[env->height];
  
  float columnSum = 0.0f;
  for (int u=0; u<env->height; u++) {
    float rowMass = 0.0f;

    for (int v = 0; v < env->width; v++) rowMass += rowCDF[u][v];

    float rowAccum = 0.0f;
    for (int v = 0; v < env->width; v++) {
      if (rowMass > 0.0f) {
        rowAccum += rowCDF[u][v] / rowMass;
        rowCDF[u][v] = rowAccum;
      } else {
        rowCDF[u][v] = 1.0f;
      }
    }

    columnSum += (sum > 0.0f) ? rowMass / sum : 0.0f;
    columnCDF[u] = columnSum;

    // float rowSum = 0;
    // for (int v=0; v<env->width; v++) {
    //   float prob = rowCDF[u][v] / sum;
    //   rowCDF[u][v] = rowSum + prob;
    //   rowSum += prob;
    // }
    // columnCDF[u] = columnSum + rowSum;
    // columnSum += rowSum;
  }
}

Vec3 EnvironmentMap::sample(const ShadingData& shadingData, Sampler* sampler, Colour& reflectedColour, float& pdf) {
  float s1 = sampler->next();
  int u = cdfBinarySearch(columnCDF, env->height, s1);

  float s2 = sampler->next();
  int v = cdfBinarySearch(rowCDF[u], env->width, s2);

  float uTex = (float)u / env->height;
  float vTex = (float)v / env->width;

  float theta = M_PI * uTex;
  float phi = 2.0f * M_PI * vTex;

  Vec3 wi = Vec3(sinf(theta) * cosf(phi), cosf(theta), sinf(theta) * sinf(phi));
  pdf = PDF(shadingData, wi);
  reflectedColour = evaluate(wi);

  return wi;
}

Colour EnvironmentMap::evaluate(const Vec3& wi) {
  float u = atan2f(wi.z, wi.x);
  u = (u < 0.0f) ? u + (2.0f * M_PI) : u;
  u = u / (2.0f * M_PI);
  float v = acosf(std::clamp(wi.y, -1.0f, 1.0f)) / M_PI;
  return env->sample(u, v);
}

// p(ω) = lum(u,v) · W·H / (totalSum · 2π²)
// Derivation: weight = lum·sin(θ), pixel solid angle = sin(θ)·2π²/(W·H) → sin(θ) cancels.
float EnvironmentMap::PDF(const ShadingData& shadingData, const Vec3& wi) {
  float u = atan2f(wi.z, wi.x);
  u = (u < 0.0f) ? u + (2.0f * M_PI) : u;
  u = u / (2.0f * M_PI);

  float theta = acosf(std::clamp(wi.y, -1.0f, 1.0f));
  float v = theta / M_PI;

  if (totalSum < 1e-10f) return 0.0f; // fully black environment

  Colour color = env->sample(u, v);
  float lum = color.lum();

  return lum * (float)(env->width * env->height) / (totalSum * 2.0f * M_PI * M_PI);
}

bool EnvironmentMap::isArea() {
  return false;
}

Vec3 EnvironmentMap::normal(const ShadingData& shadingData, const Vec3& wi) {
  return -wi;
}

float EnvironmentMap::totalIntegratedPower() {
  if (_cachedTotalPower != -1) return _cachedTotalPower;

  float total = 0;
  for (int i = 0; i < env->height; i++) {
    float st = sinf(((float)i / (float)env->height) * M_PI);
    for (int n = 0; n < env->width; n++) {
      total += (env->texels[(i * env->width) + n].lum() * st);
    }
  }

  total = total / (float)(env->width * env->height);
  total = total * 4.0f * M_PI;

  _cachedTotalPower = total;
  return total;
}

Vec3 EnvironmentMap::samplePositionFromLight(Sampler* sampler, float& pdf) {
  Vec3 p = SamplingDistributions::uniformSampleSphere(sampler->next(), sampler->next());
  p = p * sceneRadius + sceneCentre;
  pdf = 1.0f / (4.0f * M_PI * sceneRadius * sceneRadius);
  return p;
}

Vec3 EnvironmentMap::sampleDirectionFromLight(Sampler* sampler, float& pdf) {
  float s1 = sampler->next();
  int u = cdfBinarySearch(columnCDF, env->height, s1);

  float s2 = sampler->next();
  int v = cdfBinarySearch(rowCDF[u], env->width, s2);

  float uTex = (float)u / env->height;
  float vTex = (float)v / env->width;

  float theta = M_PI * uTex;
  float phi = 2.0f * M_PI * vTex;

  Vec3 wi = Vec3(sinf(theta) * cosf(phi), cosf(theta), sinf(theta) * sinf(phi));
  // Texture::sample takes (tu = azimuth/column, tv = polar/row): vTex is the
  // column coordinate, uTex the row. Passing them swapped read the pdf from
  // an unrelated texel, so sun-bright photons could get a dim-texel pdf —
  // the 1e8x emission monsters in env scenes (found 2026-08-30).
  Colour color = env->sample(vTex, uTex);
  pdf = (totalSum < 1e-10f) ? 0.0f : color.lum() * (float)(env->width * env->height) / (totalSum * 2.0f * M_PI * M_PI);

  return wi;
}
