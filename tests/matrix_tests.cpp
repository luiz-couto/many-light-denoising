#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <stdexcept>
#include "core.h"

// Helper: check two matrices are equal within floating point tolerance
static void requireMatrixEq(const Matrix& a, const Matrix& b) {
  for (int i = 0; i < 16; i++)
    REQUIRE(a.m[i] == Catch::Approx(b.m[i]).margin(1e-4f));
}

// Helper: check a Vec3 matches expected values
static void requireVecEq(const Vec3& v, float x, float y, float z) {
  REQUIRE(v.x == Catch::Approx(x).margin(1e-4f));
  REQUIRE(v.y == Catch::Approx(y).margin(1e-4f));
  REQUIRE(v.z == Catch::Approx(z).margin(1e-4f));
}

// -----------------------------------------------------------------------
// Identity
// -----------------------------------------------------------------------

TEST_CASE("Matrix default constructor is identity") {
  Matrix m;
  REQUIRE(m.m[0]  == 1.0f); REQUIRE(m.m[1]  == 0.0f);
  REQUIRE(m.m[2]  == 0.0f); REQUIRE(m.m[3]  == 0.0f);
  REQUIRE(m.m[4]  == 0.0f); REQUIRE(m.m[5]  == 1.0f);
  REQUIRE(m.m[6]  == 0.0f); REQUIRE(m.m[7]  == 0.0f);
  REQUIRE(m.m[8]  == 0.0f); REQUIRE(m.m[9]  == 0.0f);
  REQUIRE(m.m[10] == 1.0f); REQUIRE(m.m[11] == 0.0f);
  REQUIRE(m.m[12] == 0.0f); REQUIRE(m.m[13] == 0.0f);
  REQUIRE(m.m[14] == 0.0f); REQUIRE(m.m[15] == 1.0f);
}

TEST_CASE("Matrix identity * M == M") {
  Matrix identity;
  Matrix m(
    1, 2, 3, 4,
    5, 6, 7, 8,
    9, 10, 11, 12,
    13, 14, 15, 16
  );
  requireMatrixEq(identity * m, m);
  requireMatrixEq(m * identity, m);
}

// -----------------------------------------------------------------------
// Transpose
// -----------------------------------------------------------------------

TEST_CASE("Matrix transpose") {
  SECTION("transpose of identity is identity") {
    Matrix m;
    requireMatrixEq(m.transpose(), m);
  }

  SECTION("rows become columns") {
    Matrix m(
      1,  2,  3,  4,
      5,  6,  7,  8,
      9,  10, 11, 12,
      13, 14, 15, 16
    );
    Matrix t = m.transpose();
    REQUIRE(t.a[0][1] == Catch::Approx(m.a[1][0]));
    REQUIRE(t.a[0][2] == Catch::Approx(m.a[2][0]));
    REQUIRE(t.a[1][2] == Catch::Approx(m.a[2][1]));
    REQUIRE(t.a[0][3] == Catch::Approx(m.a[3][0]));
  }

  SECTION("transpose twice returns original") {
    Matrix m(
      1, 2, 3, 4,
      5, 6, 7, 8,
      9, 10, 11, 12,
      13, 14, 15, 16
    );
    requireMatrixEq(m.transpose().transpose(), m);
  }
}

// -----------------------------------------------------------------------
// Translation
// -----------------------------------------------------------------------

TEST_CASE("Matrix translation") {
  SECTION("translating origin gives translation vector") {
    Matrix t = Matrix::translation(Vec3(1.0f, 2.0f, 3.0f));
    Vec3 origin(0.0f, 0.0f, 0.0f);
    requireVecEq(t.mulPoint(origin), 1.0f, 2.0f, 3.0f);
  }

  SECTION("translating a point") {
    Matrix t = Matrix::translation(Vec3(5.0f, -3.0f, 1.0f));
    Vec3 p(1.0f, 1.0f, 1.0f);
    requireVecEq(t.mulPoint(p), 6.0f, -2.0f, 2.0f);
  }

  SECTION("translation does NOT affect directions") {
    Matrix t = Matrix::translation(Vec3(1.0f, 2.0f, 3.0f));
    Vec3 dir(1.0f, 0.0f, 0.0f);
    requireVecEq(t.mulVec(dir), 1.0f, 0.0f, 0.0f);
  }
}

// -----------------------------------------------------------------------
// Scaling
// -----------------------------------------------------------------------

TEST_CASE("Matrix scaling") {
  SECTION("scaling a vector") {
    Matrix s = Matrix::scaling(Vec3(2.0f, 3.0f, 4.0f));
    Vec3 v(1.0f, 1.0f, 1.0f);
    requireVecEq(s.mulVec(v), 2.0f, 3.0f, 4.0f);
  }

  SECTION("uniform scale of 1 leaves vector unchanged") {
    Matrix s = Matrix::scaling(Vec3(1.0f, 1.0f, 1.0f));
    Vec3 v(3.0f, 5.0f, 7.0f);
    requireVecEq(s.mulVec(v), 3.0f, 5.0f, 7.0f);
  }
}

// -----------------------------------------------------------------------
// mulVec vs mulPoint
// -----------------------------------------------------------------------

TEST_CASE("mulVec ignores translation, mulPoint applies it") {
  Matrix t = Matrix::translation(Vec3(10.0f, 20.0f, 30.0f));
  Vec3 v(1.0f, 1.0f, 1.0f);

  Vec3 asDir   = t.mulVec(v);
  Vec3 asPoint = t.mulPoint(v);

  requireVecEq(asDir,   1.0f,  1.0f,  1.0f);
  requireVecEq(asPoint, 11.0f, 21.0f, 31.0f);
}

// -----------------------------------------------------------------------
// Rotation
// -----------------------------------------------------------------------

TEST_CASE("Matrix rotateX") {
  float angle = PI / 2.0f;

  SECTION("X axis unchanged") {
    Vec3 x(1.0f, 0.0f, 0.0f);
    requireVecEq(Matrix::rotateX(angle).mulVec(x), 1.0f, 0.0f, 0.0f);
  }

  SECTION("Y axis rotates to -Z") {
    Vec3 y(0.0f, 1.0f, 0.0f);
    requireVecEq(Matrix::rotateX(angle).mulVec(y), 0.0f, 0.0f, -1.0f);
  }

  SECTION("Z axis rotates to +Y") {
    Vec3 z(0.0f, 0.0f, 1.0f);
    requireVecEq(Matrix::rotateX(angle).mulVec(z), 0.0f, 1.0f, 0.0f);
  }
}

TEST_CASE("Matrix rotateY") {
  float angle = PI / 2.0f;

  SECTION("Y axis unchanged") {
    Vec3 y(0.0f, 1.0f, 0.0f);
    requireVecEq(Matrix::rotateY(angle).mulVec(y), 0.0f, 1.0f, 0.0f);
  }

  SECTION("X axis rotates to +Z") {
    Vec3 x(1.0f, 0.0f, 0.0f);
    requireVecEq(Matrix::rotateY(angle).mulVec(x), 0.0f, 0.0f, 1.0f);
  }

  SECTION("Z axis rotates to -X") {
    Vec3 z(0.0f, 0.0f, 1.0f);
    requireVecEq(Matrix::rotateY(angle).mulVec(z), -1.0f, 0.0f, 0.0f);
  }
}

TEST_CASE("Matrix rotateZ") {
  float angle = PI / 2.0f;

  SECTION("Z axis unchanged") {
    Vec3 z(0.0f, 0.0f, 1.0f);
    requireVecEq(Matrix::rotateZ(angle).mulVec(z), 0.0f, 0.0f, 1.0f);
  }

  SECTION("X axis rotates to -Y") {
    Vec3 x(1.0f, 0.0f, 0.0f);
    requireVecEq(Matrix::rotateZ(angle).mulVec(x), 0.0f, -1.0f, 0.0f);
  }

  SECTION("Y axis rotates to +X") {
    Vec3 y(0.0f, 1.0f, 0.0f);
    requireVecEq(Matrix::rotateZ(angle).mulVec(y), 1.0f, 0.0f, 0.0f);
  }
}

TEST_CASE("Rotation by 0 leaves vector unchanged") {
  Vec3 v(1.0f, 2.0f, 3.0f);
  requireVecEq(Matrix::rotateX(0.0f).mulVec(v), 1.0f, 2.0f, 3.0f);
  requireVecEq(Matrix::rotateY(0.0f).mulVec(v), 1.0f, 2.0f, 3.0f);
  requireVecEq(Matrix::rotateZ(0.0f).mulVec(v), 1.0f, 2.0f, 3.0f);
}

TEST_CASE("Rotation by 2*PI returns to original") {
  Vec3 v(1.0f, 2.0f, 3.0f);
  requireVecEq(Matrix::rotateX(2.0f * PI).mulVec(v), 1.0f, 2.0f, 3.0f);
  requireVecEq(Matrix::rotateY(2.0f * PI).mulVec(v), 1.0f, 2.0f, 3.0f);
  requireVecEq(Matrix::rotateZ(2.0f * PI).mulVec(v), 1.0f, 2.0f, 3.0f);
}

// -----------------------------------------------------------------------
// Invert
// -----------------------------------------------------------------------

TEST_CASE("Matrix invert") {
  SECTION("M * invert(M) == identity") {
    Matrix t = Matrix::translation(Vec3(1.0f, 2.0f, 3.0f));
    requireMatrixEq(t * t.invert(), Matrix());
  }

  SECTION("invert of identity is identity") {
    requireMatrixEq(Matrix().invert(), Matrix());
  }

  SECTION("invert of rotation == transpose of rotation") {
    Matrix r = Matrix::rotateX(0.3f);
    requireMatrixEq(r.invert(), r.transpose());
  }

  SECTION("invert twice returns original") {
    Matrix t = Matrix::translation(Vec3(4.0f, 5.0f, 6.0f));
    requireMatrixEq(t.invert().invert(), t);
  }

  SECTION("singular matrix throws") {
    Matrix singular(
      0, 0, 0, 0,
      0, 0, 0, 0,
      0, 0, 0, 0,
      0, 0, 0, 0
    );
    REQUIRE_THROWS_AS(singular.invert(), std::runtime_error);
  }
}

// -----------------------------------------------------------------------
// Composed transforms
// -----------------------------------------------------------------------

TEST_CASE("Scale then translate") {
  Matrix s = Matrix::scaling(Vec3(2.0f, 2.0f, 2.0f));
  Matrix t = Matrix::translation(Vec3(1.0f, 0.0f, 0.0f));
  Vec3 p(1.0f, 0.0f, 0.0f);

  // Scale first, then translate: point (1,0,0) -> scale -> (2,0,0) -> translate -> (3,0,0)
  requireVecEq((t * s).mulPoint(p), 3.0f, 0.0f, 0.0f);
}

TEST_CASE("Rotation composed with itself") {
  // Two 90-degree rotations around Z should equal one 180-degree rotation
  Matrix r90  = Matrix::rotateZ(PI / 2.0f);
  Matrix r180 = Matrix::rotateZ(PI);
  requireMatrixEq(r90 * r90, r180);
}
