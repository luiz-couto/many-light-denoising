#ifndef CORE_H
#define CORE_H

#define _USE_MATH_DEFINES
#include <math.h>
#include <memory.h>
#include <vector>
#include <algorithm>

constexpr float SQ(float x) { return x * x; }

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

#endif // CORE_H
