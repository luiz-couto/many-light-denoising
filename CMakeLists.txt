cmake_minimum_required(VERSION 3.20)
project(many-light-denoising)

# Default to Release when no build type is specified — CMake's own default is
# NO optimization (-O0), which makes the renderer 10-30x slower. Override for
# debugging with: cmake -B build -DCMAKE_BUILD_TYPE=Debug
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo" "MinSizeRel")
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Homebrew prefix for SDL2 on macOS (OIDN is bundled locally below)
if(APPLE)
  list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew" "/usr/local")
endif()

# Bundled OIDN — platform-specific builds under external/
# macOS:   extract oidn-2.x.x.arm64.macos.tar.gz  → external/oidn-macos/
# Windows: extract oidn-2.x.x.x64.windows.zip     → external/oidn-windows/
if(APPLE)
  list(PREPEND CMAKE_PREFIX_PATH "${CMAKE_SOURCE_DIR}/external/oidn-macos")
elseif(WIN32)
  list(PREPEND CMAKE_PREFIX_PATH "${CMAKE_SOURCE_DIR}/external/oidn-windows")
endif()

find_package(SDL2 REQUIRED)
find_package(OpenImageDenoise REQUIRED)

include(FetchContent)
FetchContent_Declare(Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v3.5.0)
FetchContent_MakeAvailable(Catch2)

add_executable(renderer src/main.cpp src/engine.cpp src/window.cpp src/core.cpp src/ray.cpp src/geometry.cpp src/sampling.cpp src/bvh.cpp src/camera.cpp src/scene.cpp src/shading.cpp src/film.cpp src/renderer.cpp src/integrator.cpp src/path_tracer.cpp src/instant_radiosity.cpp src/reservoir.cpp src/texture.cpp src/light.cpp src/material_loader.cpp)
target_include_directories(renderer PRIVATE include)
target_link_libraries(renderer PRIVATE SDL2::SDL2 OpenImageDenoise)

# Copy OIDN DLLs next to executable on Windows (required at runtime)
if(WIN32)
  add_custom_command(TARGET renderer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_SOURCE_DIR}/external/oidn-windows/bin/OpenImageDenoise.dll"
      "${CMAKE_SOURCE_DIR}/external/oidn-windows/bin/OpenImageDenoise_core.dll"
      $<TARGET_FILE_DIR:renderer>)
endif()

# Tests
add_executable(tests tests/vec3_tests.cpp tests/colour_tests.cpp tests/matrix_tests.cpp tests/frame_tests.cpp tests/vec3_utils_tests.cpp tests/spherical_tests.cpp tests/ray_tests.cpp tests/triangle_tests.cpp tests/sampling_tests.cpp tests/aabb_tests.cpp tests/bvh_tests.cpp tests/camera_tests.cpp tests/scene_tests.cpp tests/film_tests.cpp tests/renderer_tests.cpp tests/integrator_tests.cpp tests/texture_tests.cpp tests/shading_helper_tests.cpp tests/diffuse_bsdf_tests.cpp tests/mirror_bsdf_tests.cpp tests/conductor_bsdf_tests.cpp tests/glass_bsdf_tests.cpp tests/plastic_bsdf_tests.cpp tests/area_light_tests.cpp tests/background_colour_tests.cpp tests/environment_map_tests.cpp tests/path_tracer_tests.cpp tests/instant_radiosity_tests.cpp tests/reservoir_tests.cpp src/core.cpp src/ray.cpp src/geometry.cpp src/sampling.cpp src/bvh.cpp src/camera.cpp src/scene.cpp src/shading.cpp src/film.cpp src/renderer.cpp src/integrator.cpp src/path_tracer.cpp src/instant_radiosity.cpp src/reservoir.cpp src/texture.cpp src/light.cpp src/material_loader.cpp)
target_include_directories(tests PRIVATE include)
target_link_libraries(tests PRIVATE Catch2::Catch2WithMain OpenImageDenoise)
