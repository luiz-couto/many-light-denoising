#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "core.h"

TEST_CASE("Vec3 arithmetic") {
  Vec3 a(1.0f, 2.0f, 3.0f);
  Vec3 b(4.0f, 5.0f, 6.0f);

  SECTION("addition") {
    Vec3 result = a + b;
    REQUIRE(result.x == 5.0f);
    REQUIRE(result.y == 7.0f);
    REQUIRE(result.z == 9.0f);
  }

  SECTION("subtraction") {
    Vec3 result = a - b;
    REQUIRE(result.x == -3.0f);
    REQUIRE(result.y == -3.0f);
    REQUIRE(result.z == -3.0f);
  }

  SECTION("scalar multiply") {
    Vec3 result = a * 2.0f;
    REQUIRE(result.x == 2.0f);
    REQUIRE(result.y == 4.0f);
    REQUIRE(result.z == 6.0f);
  }

  SECTION("negation") {
    Vec3 result = -a;
    REQUIRE(result.x == -1.0f);
    REQUIRE(result.y == -2.0f);
    REQUIRE(result.z == -3.0f);
  }
}

TEST_CASE("Vec3 length") {
  SECTION("known length") {
    Vec3 v(0.0f, 3.0f, 4.0f);
    REQUIRE(v.length() == 5.0f);
  }

  SECTION("unit vector length is 1") {
    Vec3 v(1.0f, 0.0f, 0.0f);
    REQUIRE(v.length() == 1.0f);
  }

  SECTION("lengthSq avoids sqrt") {
    Vec3 v(0.0f, 3.0f, 4.0f);
    REQUIRE(v.lengthSq() == 25.0f);
  }
}

TEST_CASE("Vec3 normalize") {
  SECTION("normalized vector has length 1") {
    Vec3 v(1.0f, 2.0f, 3.0f);
    Vec3 n = v.normalize();
    REQUIRE(n.length() == Catch::Approx(1.0f));
  }

  SECTION("normalizing a unit vector returns itself") {
    Vec3 v(1.0f, 0.0f, 0.0f);
    Vec3 n = v.normalize();
    REQUIRE(n.x == Catch::Approx(1.0f));
    REQUIRE(n.y == Catch::Approx(0.0f));
    REQUIRE(n.z == Catch::Approx(0.0f));
  }
}

TEST_CASE("Vec3 dot product") {
  SECTION("perpendicular vectors dot to 0") {
    Vec3 a(1.0f, 0.0f, 0.0f);
    Vec3 b(0.0f, 1.0f, 0.0f);
    REQUIRE(a.dot(b) == 0.0f);
  }

  SECTION("parallel vectors dot to product of lengths") {
    Vec3 a(2.0f, 0.0f, 0.0f);
    Vec3 b(3.0f, 0.0f, 0.0f);
    REQUIRE(a.dot(b) == 6.0f);
  }
}

TEST_CASE("Vec3 cross product") {
  SECTION("X cross Y = Z") {
    Vec3 x(1.0f, 0.0f, 0.0f);
    Vec3 y(0.0f, 1.0f, 0.0f);
    Vec3 result = x.cross(y);
    REQUIRE(result.x == Catch::Approx(0.0f));
    REQUIRE(result.y == Catch::Approx(0.0f));
    REQUIRE(result.z == Catch::Approx(1.0f));
  }

  SECTION("parallel vectors cross to zero") {
    Vec3 a(1.0f, 0.0f, 0.0f);
    Vec3 b(2.0f, 0.0f, 0.0f);
    Vec3 result = a.cross(b);
    REQUIRE(result.length() == Catch::Approx(0.0f));
  }
}

TEST_CASE("Vec3 perspectiveDivide") {
  SECTION("divides xyz by w") {
    Vec3 v(2.0f, 4.0f, 6.0f, 2.0f);
    Vec3 result = v.perspectiveDivide();
    REQUIRE(result.x == Catch::Approx(1.0f));
    REQUIRE(result.y == Catch::Approx(2.0f));
    REQUIRE(result.z == Catch::Approx(3.0f));
  }
}
