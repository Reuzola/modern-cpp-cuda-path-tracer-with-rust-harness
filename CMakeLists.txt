cmake_minimum_required(VERSION 3.25)

project(PathTracer
  VERSION 0.1.0
  DESCRIPTION "Physically-based path tracer"
  LANGUAGES CXX
)

# Language extensions stay off: -std=c++20, never -std=gnu++20.
set(CMAKE_CXX_EXTENSIONS OFF)

# Standard bin/lib/include layout. Included this early because the variables it
# defines are expanded at configure time, below.
include(GNUInstallDirs)

# CTest is enabled here so test presets have something to run; the actual
# test target arrives with Catch2.
enable_testing()

# Warnings are errors in Debug only: Debug is the development loop, where the
# feedback should be strictest.
#
# Behind an option because warnings are compiler heuristics, not standard-defined:
# a newer Clang can invent a diagnostic and break a build with no code change.
# Toolchains that pin their compiler can afford an unconditional -Werror; this
# one uses whatever Clang the distro ships, so it needs an escape hatch:
#   cmake --preset dev -DPT_WARNINGS_AS_ERRORS=OFF
option(PT_WARNINGS_AS_ERRORS "Treat compiler warnings as errors in Debug builds" ON)

option(PT_ENABLE_IPO "Enable interprocedural optimization (LTO) in Release builds" ON)

# Scalar precision: the type behind pt::Float.
#
# ON  -> Float is double: the correctness-first default. Golden images and the
#        performance baseline are produced in this mode only.
# OFF -> Float is float:  half the memory traffic and twice the SIMD width,
#        which is the mode optimization and GPU CUDA phase will actually want to measure.
#
# An option rather than two build types: this is one preprocessor definition,
# not a configuration. Switching it is a re-configure of the same preset:
#   cmake --preset dev -DPT_DOUBLE_PRECISION=OFF
option(PT_DOUBLE_PRECISION "Use double as the engine's scalar type (OFF selects float)" ON)

if(PT_ENABLE_IPO)
  include(CheckIPOSupported)
  check_ipo_supported(RESULT pt_ipo_supported OUTPUT pt_ipo_error)
  if(NOT pt_ipo_supported)
    message(WARNING "IPO requested but unsupported by this toolchain: ${pt_ipo_error}")
  endif()
endif()

# ---------------------------------------------------------------------------
# pt_warnings - the project's warning policy
#
# An INTERFACE library carries no code: it is a named bundle of usage
# requirements. Targets opt in by linking it, and opt out by not linking it.
# That is the whole reason this is a target and not CMAKE_CXX_FLAGS or
# add_compile_options(), both of which apply to everything in scope with no
# way to exempt a single target later.
# ---------------------------------------------------------------------------
add_library(pt_warnings INTERFACE)
add_library(pt::warnings ALIAS pt_warnings)

# Warnings for the GCC/Clang command-line dialect.
# Chosen against a measured baseline of this codebase rather than copied from
# a template: every flag below was compiled against the tree first, and the
# comments record what it actually found.
set(pt_gnu_like_warnings
  # Baseline.
  -Wall
  -Wextra

  # Diagnose anything the standard requires a diagnostic for, and reject GNU extensions.
  -Wpedantic

  # Shadowed declarations.
  -Wshadow

  # Implicit conversions that can lose value or change signedness.
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

# MSVC speaks a different flag dialect entirely. This project targets
# WSL/Linux + Clang and there is no plan to build on Windows, but the branch is
# kept as a one-line skeleton so the structure is obvious if that ever changes.
set(pt_msvc_warnings
  /W4
)

# COMPILE_LANG_AND_ID rather than CXX_COMPILER_ID: this also filters on the
# language being compiled, so C++ warning flags are never handed to a compiler
# driver for another language.
target_compile_options(pt_warnings INTERFACE
  $<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang,AppleClang>:${pt_gnu_like_warnings}>
  $<$<COMPILE_LANG_AND_ID:CXX,MSVC>:${pt_msvc_warnings}>

  # $<CONFIG:Debug> is evaluated at generate time, not configure time, so this
  # stays correct under multi-config generators where CMAKE_BUILD_TYPE is empty.
  $<$<AND:$<BOOL:${PT_WARNINGS_AS_ERRORS}>,$<CONFIG:Debug>,$<COMPILE_LANG_AND_ID:CXX,GNU,Clang,AppleClang>>:-Werror>
  $<$<AND:$<BOOL:${PT_WARNINGS_AS_ERRORS}>,$<CONFIG:Debug>,$<COMPILE_LANG_AND_ID:CXX,MSVC>>:/WX>
)

# pt_stb - vendored third-party headers
#
# Third-party code gets a target of its own rather than an include directory
# attached to pathtracer_core. Two reasons:
#
#   1. It names the dependency. "pt::core links pt::stb" is a statement about
#      the build graph; an include directory buried inside pathtracer_core's
#      own call is invisible to anyone reading the target list.
#   2. It makes the isolation structural. SYSTEM applies per
#      target_include_directories() call, so external/ was already forced into
#      a separate call - promoting that call to a target turns an accident of
#      syntax into a boundary, and lets any other target depend on stb without
#      dragging in the entire engine.
#
# SYSTEM is the load-bearing keyword: it emits -isystem instead of -I, and the
# compiler does not diagnose code reached through -isystem. That is the only
# thing keeping ~7000 lines of 2000s-era C out from under pt::warnings, and in
# Debug out from under -Werror. The suppression is tied to the search path, not
# to the file: a relative "../../external/stb_image.h" finds the same header
# with the protection gone.
add_library(pt_stb INTERFACE)
add_library(pt::stb ALIAS pt_stb)

target_include_directories(pt_stb SYSTEM INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/external>
)

# pathtracer_core - the engine
#
# Sources are the engine's own translation units, grouped by layer. Until this
# stop the only entry was stb_impl.cpp - third-party C with no engine code in
# it - so the library archived nothing of ours. The math layer is the first
# engine code to land here.
#
# Listed directly in add_library() rather than appended with target_sources():
# there is one list, in one place, and it is trivially greppable. target_sources()
# earns its keep when sources are added from a subdirectory scope or behind a
# condition - which is what a later optional target (a viewer, say) would need,
# and what this list does not.
add_library(pathtracer_core STATIC
  src/io/image_loader.cpp
  src/io/stb_impl.cpp

  src/math/color.cpp
  src/math/random.cpp
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

  src/render/camera.cpp
  src/render/path_integrator.cpp
  src/render/renderer.cpp
  src/render/tile.cpp
)
add_library(pt::core ALIAS pathtracer_core)

# PUBLIC, not INTERFACE. The keyword now carries information it could not carry
# before: INTERFACE means "consumers only", which was the only answer available
# to a target with no sources of its own. PUBLIC means "this target's sources
# and its consumers", which is what the engine's own include root actually is.
target_include_directories(pathtracer_core PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# PUBLIC for the same reason: the library itself is compiled as C++20, and so
# is everything that links it.
target_compile_features(pathtracer_core PUBLIC cxx_std_20)

# PUBLIC, and this is the one keyword in the file that is a correctness
# requirement rather than a hygiene preference.
#
# pt/math/scalar.hpp reads this macro to decide what Float is, and that header
# is public: it is compiled once into this library's sources, and again into
# main.cpp, and again into every test TU. If those compilations disagree, the
# layout of every type containing a Float (HitRecord, Interval, Aabb, Ray)
# differs across the boundary, and every inline function in include/ has two
# different bodies under one name - an ODR violation the compiler never
# diagnoses. PRIVATE here would not be a smaller scope, it would be a bug.
#
# The generator expression rather than if(): this is evaluated at generate
# time against the cache variable, so a re-configure with -D flips it with no
# stale-branch window in between.
target_compile_definitions(pathtracer_core PUBLIC
  $<$<BOOL:${PT_DOUBLE_PRECISION}>:PT_FLOAT_AS_DOUBLE>
)

# Configure-time echo. Cheap, and it puts the one build setting that silently
# changes every rendered pixel into the configure log next to the build type.
if(PT_DOUBLE_PRECISION)
  set(PT_SCALAR_TYPE "double")
else()
  set(PT_SCALAR_TYPE "float")
endif()

message(STATUS "Scalar precision: ${CMAKE_PROJECT_NAME} Float = ${PT_SCALAR_TYPE}")

# pt::stb is PRIVATE, It was PUBLIC for one reason: image_loader.hpp
# was a public header and it included stb_image.h, which made stb's include
# directory part of this library's interface - a consumer handed our headers
# without it would fail to compile. Moving stbi_deleter::operator()'s body into
# image_loader.cpp removed the last mention of stb_image.h from include/, so the
# dependency is now an implementation detail and travels no further than this
# target's own sources.
#
# This also repairs the install rules: install(DIRECTORY include/) has been
# shipping headers, and until now those headers named a third-party
# file that is not installed with them.
#
# pt::warnings stays PRIVATE: a warning policy is a build-time developer concern,
# never something to impose on a consumer. CMake does still propagate a static
# library's PRIVATE link dependencies to consumers, but wrapped in
# $<LINK_ONLY:>, so only the link edge travels and none of the usage
# requirements do - and neither of these targets has an artifact to link.
target_link_libraries(pathtracer_core
  PRIVATE pt::stb pt::warnings
)

# pathtracer - the driver executable
# Deliberately thin: it owns main() and nothing else.
add_executable(pathtracer src/main.cpp)

# Interprocedural optimization, Release only.
#
# Set on both targets, and that pairing is the whole point: LTO removes the
# translation-unit boundary at link time, but only where both sides of that
# boundary were compiled with it. The engine's math bodies now live in
# pathtracer_core while every call site still lives in main.cpp, so enabling
# this on the executable alone would optimize across a boundary that no longer
# has anything on the other side of it.
#
# The _RELEASE suffix restricts this to the Release configuration. Without it,
# LTO would apply to Debug too - which costs link time, folds functions together,
# and produces exactly the kind of debug info that makes a stack trace lie.
#
# Deliberately not set on pathtracer_tests: nobody profiles a test binary, and
# LTO would tax every CI run with a slower link for no measurable return.
if(PT_ENABLE_IPO AND pt_ipo_supported)
  set_property(TARGET pathtracer_core PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
  set_property(TARGET pathtracer PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
endif()

# pt::core brings the engine and, transitively, stb's include path.
# pt::warnings is this target's own opt-in - see pathtracer_core above.
target_link_libraries(pathtracer PRIVATE pt::core pt::warnings)

# Tests live in their own directory scope so that the dependency they need
# (Catch2) and the CMAKE_MODULE_PATH it drags in stay contained there.
#
# Ordering is a real constraint, not style: enable_testing() above must run
# before this line. add_test() calls in a subdirectory whose parent never
# enabled testing register nothing at all - silently, with no diagnostic.
add_subdirectory(tests)

# Installation
# Deliberately minimal: the executable and the public headers, nothing else.
# pathtracer_core now produces an artifact but is still not installed - it is an
# implementation detail of the binary, and installing it usefully would mean an
# export set, a package config file, and an answer for how a consumer obtains
# stb. None of that is needed to ship a renderer.
install(TARGETS pathtracer
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Trailing slash on "include/" is load-bearing: it installs the *contents* of the directory.
install(DIRECTORY include/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  FILES_MATCHING PATTERN "*.hpp"
)
