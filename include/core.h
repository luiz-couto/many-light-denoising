#ifndef CORE_H
#define CORE_H

#define _USE_MATH_DEFINES
#include <math.h>
#include <memory.h>
#include <vector>
#include <algorithm>
#include <numbers>

constexpr float SQ(float x) { return x * x; }

constexpr float PI    = std::numbers::pi_v<float>;

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

struct Vertex {
  Vec3 p;
	Vec3 normal;
	float u, v;

  Vertex(Vec3 _p, Vec3 _normal, float _u, float _v) : p(_p), normal(_normal), u(_u), v(_v) {}
  Vertex() {}
};

class Matrix {
public:
  union {
    float a[4][4];
		float m[16];
  };

  Matrix();
  Matrix(float m00, float m01, float m02, float m03, float m10, float m11, float m12, float m13, float m20, float m21, float m22, float m23, float m30, float m31, float m32, float m33);

  float& operator[](int index);
  Matrix operator*(const Matrix& matrix) const;
  Matrix operator=(const Matrix& matrix);

  void identity();
  Matrix transpose() const;
  static Matrix translation(const Vec3& v);
  static Matrix scaling(const Vec3& v);
  Matrix mul(const Matrix& matrix) const;
  Vec3 mulVec(const Vec3& v) const;
  Vec3 mulPoint(const Vec3& v) const;
  Vec3 mulPointAndPerspectiveDivide(const Vec3& v) const;
  Matrix invert() const;
  static Matrix lookAt(const Vec3& from, const Vec3& to, const Vec3& up);

  // FOV in degrees
  static Matrix perspective(const float n, const float f, float aspect, const float fov);

  static Matrix rotateX(float theta);
  static Matrix rotateY(float theta);
  static Matrix rotateZ(float theta);
};

#endif // CORE_H
