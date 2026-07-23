cmake_minimum_required(VERSION 3.20)
project(many-light-denoising)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Homebrew prefix so find_package works on macOS (both Apple Silicon and Intel)
if(APPLE)
  list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew" "/usr/local")
endif()

# Dependencies
find_package(SDL2 REQUIRED)
find_package(OpenImageDenoise REQUIRED)

include(FetchContent)
FetchContent_Declare(Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v3.5.0)
FetchContent_MakeAvailable(Catch2)

add_executable(renderer src/main.cpp src/engine.cpp src/window.cpp src/core.cpp)
target_include_directories(renderer PRIVATE include)
target_link_libraries(renderer PRIVATE SDL2::SDL2 OpenImageDenoise)

# Tests
add_executable(tests tests/vec3_tests.cpp tests/colour_tests.cpp tests/matrix_tests.cpp tests/frame_tests.cpp tests/vec3_utils_tests.cpp tests/spherical_tests.cpp tests/ray_tests.cpp tests/triangle_tests.cpp tests/sampling_tests.cpp tests/aabb_tests.cpp tests/bvh_tests.cpp tests/camera_tests.cpp tests/scene_tests.cpp src/core.cpp src/ray.cpp src/geometry.cpp src/sampling.cpp src/bvh.cpp src/camera.cpp src/scene.cpp src/shading.cpp)
target_include_directories(tests PRIVATE include)
target_link_libraries(tests PRIVATE Catch2::Catch2WithMain)
