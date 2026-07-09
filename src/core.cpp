#include "core.h"

Vec3::Vec3() {
  x = 0; y = 0; z = 0;
  w = 1.0f;
}

Vec3::Vec3(float _x, float _y, float _z):
x(_x), y(_y), z(_z) {
  w = 1.0f;
}

Vec3::Vec3(float _x, float _y, float _z, float _w):
x(_x), y(_y), z(_z), w(_w) {}

Vec3 Vec3::operator+(const Vec3 &v) const {
  return Vec3(x + v.x, y + v.y, z + v.z);
}

Vec3 Vec3::operator-(const Vec3 &v) const {
  return Vec3(x - v.x, y - v.y, z - v.z);
}

Vec3 Vec3::operator*(const float &v) const {
  return Vec3(x * v, y * v, z * v);
}

Vec3 Vec3::operator/(const float &v) const {
  return Vec3(x / v, y / v, z / v, w / v);
}

Vec3 Vec3::operator*(const Vec3 &v) const {
  return Vec3(x * v.x, y * v.y, z * v.z);
}

Vec3 Vec3::operator-() const {
  return Vec3(-x, -y, -z);
}

Vec3 Vec3::perspectiveDivide() const {
  return Vec3(x / w, y / w, z / w, 1.0f / w);
}

float Vec3::length() const {
  return sqrtf((x * x) + (y * y) + (z * z));
}

float Vec3::lengthSq() const {
  return ((x * x) + (y * y) + (z * z));
}

Vec3 Vec3::normalize() const {
  float inverseLen = 1.0f / sqrtf((x * x) + (y * y) + (z * z));
	return Vec3(x * inverseLen, y * inverseLen, z * inverseLen);
}

float Vec3::dot(Vec3 v) const {
  return ((x * v.x) + (y * v.y) + (z * v.z));
}

Vec3 Vec3::cross(Vec3 v) const {
  return Vec3(
    (y * v.z) - (z * v.y), 
    (z * v.x) - (x * v.z), 
    (x * v.y) - (y * v.x)
  );
}

Colour::Colour() {
  r = 0; g = 0; b = 0;
}

Colour::Colour(float _r, float _g, float _b):
r(_r), g(_g), b(_b) {}

Colour::Colour(unsigned char _r, unsigned char _g, unsigned char _b) {
  r = (float)_r / 255.0f;
	g = (float)_g / 255.0f;
	b = (float)_b / 255.0f;
}

Colour Colour::operator+(const Colour& colour) const {
  Colour c;
  c.r = r + colour.r;
  c.g = g + colour.g;
  c.b = b + colour.b;
	return c;
}

Colour Colour::operator-(const Colour& colour) const {
  Colour c;
  c.r = r - colour.r;
  c.g = g - colour.g;
  c.b = b - colour.b;
  return c;
}

Colour Colour::operator*(const Colour& colour) const {
  Colour c;
  c.r = r * colour.r;
  c.g = g * colour.g;
  c.b = b * colour.b;
  return c;
}

Colour Colour::operator*(const float& num) const {
  Colour c;
  c.r = r * num;
  c.g = g * num;
  c.b = b * num;
  return c;
}

Colour Colour::operator/(const Colour& colour) const {
  Colour c;
  c.r = r / colour.r;
  c.g = g / colour.g;
  c.b = b / colour.b;
  return c;
}

Colour Colour::operator/(const float &v) const {
  Colour c;
  c.r = r / v;
  c.g = g / v;
  c.b = b / v;
  return c;
}

void Colour::toRGB(unsigned char& cr, unsigned char& cg, unsigned char& cb) const {
  cr = (unsigned char)(std::clamp(r, 0.0f, 1.0f) * 255);
  cg = (unsigned char)(std::clamp(g, 0.0f, 1.0f) * 255);
  cb = (unsigned char)(std::clamp(b, 0.0f, 1.0f) * 255);
}

float Colour::lum() const {
  return (LUM_R * r) + (LUM_G * g) + (LUM_B * b);
}
