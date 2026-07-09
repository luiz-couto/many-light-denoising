#ifndef CORE_H
#define CORE_H

#define _USE_MATH_DEFINES
#include <math.h>
#include <memory.h>
#include <vector>
#include <algorithm>

constexpr float SQ(float x) { return x * x; }

constexpr float LUM_R = 0.2126f;
constexpr float LUM_G = 0.7152f;
constexpr float LUM_B = 0.0722f;

class Vec3 {
public:
  union {
		struct {
			float x;
			float y;
			float z;
			float w;
		};
		float coords[4];
	};

  Vec3();
  Vec3(float _x, float _y, float _z);
  Vec3(float _x, float _y, float _z, float _w);
  
  Vec3 operator+(const Vec3 &v) const;
  Vec3 operator-(const Vec3 &v) const;
  Vec3 operator*(const float &v) const;
  Vec3 operator/(const float &v) const;
  Vec3 operator*(const Vec3 &v) const;
  Vec3 operator-() const;
  
  Vec3 perspectiveDivide() const;
  float length() const;
  float lengthSq() const;
  Vec3 normalize() const;
  float dot(Vec3 v) const;
  Vec3 cross(Vec3 v) const;
};

class Colour {
public:
  float r, g, b;

  Colour();
  Colour(float _r, float _g, float _b);
  Colour(unsigned char _r, unsigned char _g, unsigned char _b);

  Colour operator+(const Colour& colour) const;
  Colour operator-(const Colour& colour) const;
  Colour operator*(const Colour& colour) const;
  Colour operator*(const float& num) const;
  Colour operator/(const Colour& colour) const;
  Colour operator/(const float &v) const;
  
  void toRGB(unsigned char& cr, unsigned char& cg, unsigned char& cb) const;
  float lum() const;
};

#endif // CORE_H
