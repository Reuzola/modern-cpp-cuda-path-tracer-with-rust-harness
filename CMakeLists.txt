cmake_minimum_required(VERSION 3.25)

project(PathTracer
  VERSION 0.1.0
  DESCRIPTION "Physically-based path tracer"
  LANGUAGES CXX
)

# GNU extensions off: -std=c++20, never -std=gnu++20.
set(CMAKE_CXX_EXTENSIONS OFF)

# Standard bin/lib/include layout.
include(GNUInstallDirs)

# Enables CTest so test presets have something to run.
enable_testing()

# Warnings-as-errors in Debug only. Off by default escape hatch:
#   cmake --preset dev -DPT_WARNINGS_AS_ERRORS=OFF
option(PT_WARNINGS_AS_ERRORS "Treat compiler warnings as errors in Debug builds" ON)

option(PT_ENABLE_IPO "Enable interprocedural optimization (LTO) in Release builds" ON)

# Scalar precision behind pt::Float.
# ON  -> double (correctness-first default; golden images use this).
# OFF -> float (half memory traffic, wider SIMD; used for perf/GPU work).
#   cmake --preset dev -DPT_DOUBLE_PRECISION=OFF
option(PT_DOUBLE_PRECISION "Use double as the engine's scalar type (OFF selects float)" ON)

if(PT_ENABLE_IPO)
  include(CheckIPOSupported)
  check_ipo_supported(RESULT pt_ipo_supported OUTPUT pt_ipo_error)
  if(NOT pt_ipo_supported)
    message(WARNING "IPO requested but unsupported by this toolchain: ${pt_ipo_error}")
  endif()
endif()

# pt_warnings - project warning policy, as an INTERFACE library so targets
# can opt in/out by linking (unlike CMAKE_CXX_FLAGS, which is global)
add_library(pt_warnings INTERFACE)
add_library(pt::warnings ALIAS pt_warnings)

# GCC/Clang warning flags, tuned against this codebase.
set(pt_gnu_like_warnings
  # Baseline.
  -Wall
  -Wextra

  # Standard-required diagnostics; reject GNU extensions.
  -Wpedantic

  # Shadowed declarations.
  -Wshadow

  # Implicit conversions that lose value or change signedness.
  -Wconversion
  -Wsign-conversion

  # Silent float -> double widening.
  -Wdouble-promotion

  # Polymorphic-hierarchy correctness.
  -Wnon-virtual-dtor
  -Woverloaded-virtual

  # Cast discipline.
  -Wold-style-cast
  -Wcast-qual

  # Cheap correctness checks.
  -Wnull-dereference
  -Wimplicit-fallthrough
  -Wformat=2
)

# MSVC dialect skeleton; project only targets WSL/Linux + Clang for now.
set(pt_msvc_warnings
  /W4
)

# COMPILE_LANG_AND_ID also filters by language, so C++ flags never leak to other languages.
target_compile_options(pt_warnings INTERFACE
  $<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang,AppleClang>:${pt_gnu_like_warnings}>
  $<$<COMPILE_LANG_AND_ID:CXX,MSVC>:${pt_msvc_warnings}>

  # $<CONFIG:Debug> is generate-time, so this stays correct under multi-config generators.
  $<$<AND:$<BOOL:${PT_WARNINGS_AS_ERRORS}>,$<CONFIG:Debug>,$<COMPILE_LANG_AND_ID:CXX,GNU,Clang,AppleClang>>:-Werror>
  $<$<AND:$<BOOL:${PT_WARNINGS_AS_ERRORS}>,$<CONFIG:Debug>,$<COMPILE_LANG_AND_ID:CXX,MSVC>>:/WX>
)

# pt_stb - vendored third-party headers, as their own target so the isolation
# is structural (SYSTEM include, -isystem suppresses diagnostics) rather than
# buried inside pathtracer_core's own include call.
add_library(pt_stb INTERFACE)
add_library(pt::stb ALIAS pt_stb)

target_include_directories(pt_stb SYSTEM INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/external>
)

# OpenEXR - HDR image output.
# CONFIG: only accept OpenEXR's own config file, never a stray FindOpenEXR.cmake.
# REQUIRED: not an optional feature, must succeed at configure time.
find_package(OpenEXR CONFIG REQUIRED)

# nlohmann_json - JSON scene file parsing.
# Package name has an underscore (vcpkg port name uses a hyphen).
# CONFIG: same reasoning as OpenEXR above.
# REQUIRED: a renderer that can't read a scene file has nothing to render.
find_package(nlohmann_json CONFIG REQUIRED)

# pathtracer_core - the engine.
# Sources listed directly in add_library() (not target_sources()) so the list
# stays in one place and is trivially greppable.
add_library(pathtracer_core STATIC
  src/util/log.cpp

  src/io/color.cpp
  src/io/exr_writer.cpp
  src/io/image_loader.cpp
  src/io/image_writer.cpp
  src/io/png_writer.cpp
  src/io/ppm_writer.cpp
  src/io/stb_impl.cpp

  src/math/color.cpp
  src/math/vec3.cpp

  src/core/hittable.cpp
  src/core/hittable_list.cpp

  src/geometry/box.cpp
  src/geometry/bvh.cpp
  src/geometry/constant_medium.cpp
  src/geometry/quad.cpp
  src/geometry/rotate_y.cpp
  src/geometry/sphere.cpp
  src/geometry/translate.cpp

  src/textures/checker_texture.cpp
  src/textures/image_texture.cpp
  src/textures/noise_texture.cpp
  src/textures/perlin.cpp
  src/textures/solid_color.cpp
  src/textures/texture.cpp

  src/materials/dielectric.cpp
  src/materials/diffuse_light.cpp
  src/materials/isotropic.cpp
  src/materials/lambertian.cpp
  src/materials/material.cpp
  src/materials/metal.cpp

  src/sampling/cosine_pdf.cpp
  src/sampling/importance_targets.cpp
  src/sampling/mixture_pdf.cpp
  src/sampling/sampleable_pdf.cpp
  src/sampling/sphere_pdf.cpp

  src/scene/scene.cpp
  src/scene/scene_loader.cpp

  src/render/camera.cpp
  src/render/path_integrator.cpp
  src/render/progress.cpp
  src/render/renderer.cpp
  src/render/tile.cpp

  src/post/tonemap.cpp
)
add_library(pt::core ALIAS pathtracer_core)

# PUBLIC: this is the engine's own include root, used by its sources and its consumers.
target_include_directories(pathtracer_core PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# PUBLIC: the library and everything linking it compile as C++20.
target_compile_features(pathtracer_core PUBLIC cxx_std_20)

# PUBLIC and load-bearing: pt/math/scalar.hpp reads this macro to define Float,
# and that header is compiled into this library, main.cpp, and every test TU.
# A mismatch here is an ODR violation the compiler won't catch.
target_compile_definitions(pathtracer_core PUBLIC
  $<$<BOOL:${PT_DOUBLE_PRECISION}>:PT_FLOAT_AS_DOUBLE>
)

# Configure-time echo of the scalar type, since it silently affects every pixel.
if(PT_DOUBLE_PRECISION)
  set(PT_SCALAR_TYPE "double")
else()
  set(PT_SCALAR_TYPE "float")
endif()

message(STATUS "Scalar precision: ${CMAKE_PROJECT_NAME} Float = ${PT_SCALAR_TYPE}")

# pt::stb, pt::warnings, OpenEXR::OpenEXR, nlohmann_json::nlohmann_json: all PRIVATE.
# None of their types appear in this library's public headers (include/), so
# none of these dependencies should leak to consumers. CMake still propagates
# a static library's PRIVATE link deps via $<LINK_ONLY:>, so the link edge
# reaches consumers even though the usage requirements don't.
target_link_libraries(pathtracer_core
  PRIVATE pt::stb pt::warnings OpenEXR::OpenEXR nlohmann_json::nlohmann_json
)

# CLI11 - command-line argument parsing, found here next to its consumer
# (the driver) rather than up with OpenEXR/nlohmann_json (the engine's deps).
# CONFIG: same reasoning as above. Header-only, so linking just carries usage requirements.
find_package(CLI11 CONFIG REQUIRED)

# pathtracer - the driver executable. Deliberately thin: owns main() and the CLI only.
add_executable(pathtracer
  src/main.cpp
  src/app/cli.cpp
)

# IPO/LTO, Release only, set on both targets since LTO needs both sides of a
# TU boundary compiled with it. pathtracer_tests needs it too and sets it in
# tests/CMakeLists.txt, where the target exists.
if(PT_ENABLE_IPO AND pt_ipo_supported)
  set_property(TARGET pathtracer_core PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
  set_property(TARGET pathtracer PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
endif()

# Private include root for src/, so app/cli.hpp is reachable without living in
# include/ (which is public and installed).
target_include_directories(pathtracer PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# pt::core brings the engine (and transitively stb's include path).
# CLI11::CLI11 is PRIVATE: an executable has no consumers to propagate to.
# Its include dir arrives as SYSTEM automatically, keeping it out from under
# -Wconversion/-Wold-style-cast and -Werror.
target_link_libraries(pathtracer PRIVATE pt::core pt::warnings CLI11::CLI11)

# Tests get their own directory scope so Catch2 (and the CMAKE_MODULE_PATH it
# adds) stays contained there. Must come after enable_testing() above.
add_subdirectory(tests)

# Installation: executable + public headers only. pathtracer_core is not
# installed - it's an implementation detail of the binary.
install(TARGETS pathtracer
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Trailing slash on "include/" installs the directory's *contents*.
install(DIRECTORY include/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  FILES_MATCHING PATTERN "*.hpp"
)