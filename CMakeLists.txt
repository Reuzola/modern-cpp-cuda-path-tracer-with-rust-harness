cmake_minimum_required(VERSION 3.25)

project(PathTracer
  VERSION 1.0.0
  DESCRIPTION "Physically-based path tracer"
  LANGUAGES CXX
)

# No GNU extensions: -std=c++20, never gnu++20.
set(CMAKE_CXX_EXTENSIONS OFF)

# Standard bin/lib/include layout.
include(GNUInstallDirs)

# Needed for test presets.
enable_testing()

# Debug-only warnings-as-errors. Escape hatch:
#   cmake --preset dev -DPT_WARNINGS_AS_ERRORS=OFF
option(PT_WARNINGS_AS_ERRORS "Treat compiler warnings as errors in Debug builds" ON)

# Release-only IPO/LTO across translation units. Escape hatch:
#   cmake --preset release -DPT_ENABLE_IPO=OFF
option(PT_ENABLE_IPO "Enable interprocedural optimization (LTO) in Release builds" ON)

# pt::Float precision. ON=double (default, golden images), OFF=float (perf/GPU).
#   cmake --preset dev -DPT_DOUBLE_PRECISION=OFF
option(PT_DOUBLE_PRECISION "Use double as the engine's scalar type (OFF selects float)" ON)

# BVH traversal counters; off by default (hot-loop cost).
#   cmake --preset release-stats
option(PT_ENABLE_STATS "Collect BVH traversal statistics" OFF)

# Interactive GLFW viewer. OFF by default: headless CI, golden renders and
# release builds must not pull in a windowing/OpenGL stack.
#   cmake --preset dev -DPT_BUILD_VIEWER=ON
option(PT_BUILD_VIEWER "Build the interactive viewer executable" OFF)

if(PT_ENABLE_IPO)
  include(CheckIPOSupported)
  check_ipo_supported(RESULT pt_ipo_supported OUTPUT pt_ipo_error)
  if(NOT pt_ipo_supported)
    message(WARNING "IPO requested but unsupported by this toolchain: ${pt_ipo_error}")
  endif()
endif()

# INTERFACE lib so targets opt in/out via linking, unlike global CMAKE_CXX_FLAGS.
add_library(pt_warnings INTERFACE)
add_library(pt::warnings ALIAS pt_warnings)

# GCC/Clang warnings, tuned for this codebase.
set(pt_gnu_like_warnings
  # Baseline.
  -Wall
  -Wextra

  # Reject GNU extensions.
  -Wpedantic

  # Shadowed declarations.
  -Wshadow

  # Value/sign-losing conversions.
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

# Clang-only warnings (GCC errors on unknown flags).
set(pt_clang_only_warnings
  # Extended shadowing (superset of -Wshadow).
  -Wshadow-all
)

# MSVC skeleton; project currently targets WSL/Linux + Clang only.
set(pt_msvc_warnings
  /W4
)

# COMPILE_LANG_AND_ID filters by language too, so C++ flags don't leak elsewhere.
target_compile_options(pt_warnings INTERFACE
  $<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang,AppleClang>:${pt_gnu_like_warnings}>
  $<$<COMPILE_LANG_AND_ID:CXX,Clang,AppleClang>:${pt_clang_only_warnings}>
  $<$<COMPILE_LANG_AND_ID:CXX,MSVC>:${pt_msvc_warnings}>

  # Generate-time check; stays correct under multi-config generators.
  $<$<AND:$<BOOL:${PT_WARNINGS_AS_ERRORS}>,$<CONFIG:Debug>,$<COMPILE_LANG_AND_ID:CXX,GNU,Clang,AppleClang>>:-Werror>
  $<$<AND:$<BOOL:${PT_WARNINGS_AS_ERRORS}>,$<CONFIG:Debug>,$<COMPILE_LANG_AND_ID:CXX,MSVC>>:/WX>
)

# Vendored headers, isolated via SYSTEM include (-isystem suppresses diagnostics).
add_library(pt_stb INTERFACE)
add_library(pt::stb ALIAS pt_stb)

target_include_directories(pt_stb SYSTEM INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/external>
)

# OpenEXR - HDR output. CONFIG only (no stray FindOpenEXR.cmake). REQUIRED.
find_package(OpenEXR CONFIG REQUIRED)

# nlohmann_json - scene JSON parsing (vcpkg port uses a hyphen). REQUIRED.
find_package(nlohmann_json CONFIG REQUIRED)

# tinyobjloader - OBJ parsing. REQUIRED: mesh loading is compiled-in.
find_package(tinyobjloader CONFIG REQUIRED)

# pathtracer_core - the engine. Sources listed inline for greppability.
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
  src/geometry/instance.cpp
  src/geometry/mesh.cpp
  src/geometry/quad.cpp
  src/geometry/sphere.cpp
  src/geometry/triangle.cpp

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

  src/scene/obj_loader.cpp
  src/scene/scene.cpp
  src/scene/scene_error.cpp
  src/scene/scene_loader.cpp

  src/render/camera.cpp
  src/render/path_integrator.cpp
  src/render/progress.cpp
  src/render/renderer.cpp
  src/render/tile.cpp

  src/post/tonemap.cpp
)
add_library(pt::core ALIAS pathtracer_core)

# Engine's own include root, used by its sources and consumers.
target_include_directories(pathtracer_core PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# PUBLIC: consumers also compile as C++20.
target_compile_features(pathtracer_core PUBLIC cxx_std_20)

# Load-bearing: PT_FLOAT_AS_DOUBLE sets Float, PT_STATS gates inline bodies.
# Must match across all TUs or it's an undetected ODR violation.
target_compile_definitions(pathtracer_core PUBLIC
  $<$<BOOL:${PT_DOUBLE_PRECISION}>:PT_FLOAT_AS_DOUBLE>
  $<$<BOOL:${PT_ENABLE_STATS}>:PT_STATS>
)

# Echo scalar type at configure time (it affects every pixel).
if(PT_DOUBLE_PRECISION)
  set(PT_SCALAR_TYPE "double")
else()
  set(PT_SCALAR_TYPE "float")
endif()

message(STATUS "Scalar precision: ${CMAKE_PROJECT_NAME} Float = ${PT_SCALAR_TYPE}")

# All PRIVATE: none of these types appear in public headers. Note that a
# static lib's PRIVATE deps still reach consumers via $<LINK_ONLY:>.
target_link_libraries(pathtracer_core
  PRIVATE pt::stb pt::warnings OpenEXR::OpenEXR nlohmann_json::nlohmann_json tinyobjloader::tinyobjloader
)

# CLI11 - kept near its consumer (the driver), not with the engine's deps.
find_package(CLI11 CONFIG REQUIRED)

# pathtracer - driver executable. Deliberately thin: main() + CLI only.
add_executable(pathtracer
  src/main.cpp
  src/app/cli.cpp
  src/app/benchmark.cpp
)

# Release-only IPO/LTO; set on both sides of the TU boundary.
# pathtracer_tests sets it too, in tests/CMakeLists.txt.
if(PT_ENABLE_IPO AND pt_ipo_supported)
  set_property(TARGET pathtracer_core PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
  set_property(TARGET pathtracer PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
endif()

# Private src/ include root, so app/cli.hpp needn't live in public include/.
target_include_directories(pathtracer PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# pt::core brings the engine (+ stb path). CLI11 is PRIVATE (no consumers);
# its include dir is SYSTEM, so -Wconversion/-Werror skip it.
target_link_libraries(pathtracer PRIVATE pt::core pt::warnings CLI11::CLI11 nlohmann_json::nlohmann_json)

# Source revision, written into every benchmark record so two runs taken months
# apart can be told apart. Captured at configure time; the CONFIGURE_DEPENDS
# below is what keeps it from going stale.
set(PT_GIT_REVISION "unknown")

# IS_DIRECTORY, not EXISTS: in a worktree or a submodule, .git is a file
# pointing elsewhere and the paths watched below would not exist.
if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/.git")
  find_package(Git QUIET)

  if(Git_FOUND)
    execute_process(
      COMMAND "${GIT_EXECUTABLE}" rev-parse --short=12 HEAD
      WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
      OUTPUT_VARIABLE pt_git_revision
      RESULT_VARIABLE pt_git_result
      OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_QUIET
    )

    # Fails in a repository with no commits yet; "unknown" stays.
    if(pt_git_result EQUAL 0)
      set(PT_GIT_REVISION "${pt_git_revision}")
    endif()
  endif()

  # Reconfigure when the checked-out commit moves. HEAD alone is not enough:
  # committing on the same branch rewrites the ref file, not HEAD. The index is
  # what commit, checkout, amend and rebase all touch.
  foreach(pt_git_watch "HEAD" "index")
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.git/${pt_git_watch}")
      set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/.git/${pt_git_watch}")
    endif()
  endforeach()
endif()

message(STATUS "Source revision: ${PT_GIT_REVISION}")

# Records must state which configuration produced them; a timing from a Debug
# build next to a Release one is worse than no number at all.
# Generator expression, not ${CMAKE_BUILD_TYPE}: correct under multi-config generators too.
# PT_VERSION comes from project(VERSION) so the tag, the manifests and --version
# cannot drift apart.
target_compile_definitions(pathtracer PRIVATE
  PT_BUILD_TYPE="$<CONFIG>"
  PT_VERSION="${PROJECT_VERSION}"
  PT_GIT_REVISION="${PT_GIT_REVISION}"
)

# pathtracer_viewer - interactive frontend, opt-in.
# Everything viewer-related lives inside this if(): with the option OFF, CMake
# never evaluates it, so a machine without a window stack still configures.
if(PT_BUILD_VIEWER)
  # The option and the vcpkg feature are separate switches; catch the mismatch
  # here instead of letting find_package fail with an unrelated message.
  if(NOT "viewer" IN_LIST VCPKG_MANIFEST_FEATURES)
    message(FATAL_ERROR
      "PT_BUILD_VIEWER=ON requires the 'viewer' vcpkg feature. Use: cmake --preset dev-viewer")
  endif()

  # Window and input. CONFIG only; the port's target is plain 'glfw'.
  find_package(glfw3 CONFIG REQUIRED)

  # Runtime loader for GL 3.3 entry points (only GL 1.1 is linkable directly).
  find_package(glad CONFIG REQUIRED)

  # Debug UI. The port compiles the glfw/opengl3 backends into the same target,
  # so imgui::imgui is the only thing to link.
  find_package(imgui CONFIG REQUIRED)

  add_executable(pathtracer_viewer
    src/viewer/main.cpp
    src/viewer/window.cpp
    src/viewer/display.cpp
    src/viewer/cli.cpp
    src/viewer/camera_controller.cpp
    src/viewer/gui.cpp
    src/viewer/screenshot.cpp
  )

  # Same private src/ root as the driver: viewer-local headers stay out of the
  # installed include/ tree.
  target_include_directories(pathtracer_viewer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
  )

  # Same single source as the driver's --version.
  target_compile_definitions(pathtracer_viewer PRIVATE
    PT_VERSION="${PROJECT_VERSION}"
  )

  # One-way arrow: viewer -> core. No viewer dependency ever flows back.
  # glfw/glad/imgui are imported targets, so their headers are SYSTEM: -Wconversion
  # and friends skip them.
  target_link_libraries(pathtracer_viewer PRIVATE
    pt::core
    pt::warnings
    glfw
    glad::glad
    imgui::imgui
    CLI11::CLI11
  )

  # Core uses ThinLTO in Release; linking targets must opt into IPO too.
  if(PT_ENABLE_IPO AND pt_ipo_supported)
    set_property(TARGET pathtracer_viewer PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
  endif()

  install(TARGETS pathtracer_viewer
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  )
endif()

# Own scope so Catch2 stays contained. Must follow enable_testing() above.
add_subdirectory(tests)

# Install executable + public headers only; pathtracer_core stays internal.
install(TARGETS pathtracer
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Trailing slash installs the directory's *contents*.
install(DIRECTORY include/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  FILES_MATCHING PATTERN "*.hpp"
)
