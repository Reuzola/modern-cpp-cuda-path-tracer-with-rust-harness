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

# pathtracer_core - the engine
# INTERFACE, not STATIC: the codebase is still header-only, so there is no
# object code to archive. This becomes STATIC once the engine gains its first
# translation unit (src/io/stb_impl.cpp)
add_library(pathtracer_core INTERFACE)
add_library(pt::core ALIAS pathtracer_core)

# Our own headers: not SYSTEM, so pt::warnings applies to every one of them.
target_include_directories(pathtracer_core INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# Vendored third-party headers, marked SYSTEM so the compiler stops diagnosing
# code we do not own. SYSTEM applies to the whole call, not to individual
# directories, which is why this is a separate call from the one above:
# folding external/ into it would silence our own headers too.
target_include_directories(pathtracer_core SYSTEM INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/external>
)

# Anything linking pathtracer_core is compiled as C++20 or later.
target_compile_features(pathtracer_core INTERFACE cxx_std_20)

# pathtracer - the driver executable
# Deliberately thin: it owns main() and nothing else.
add_executable(pathtracer src/main.cpp)

if(PT_ENABLE_IPO AND pt_ipo_supported)
  set_property(TARGET pathtracer PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
endif()

# pt::warnings is PRIVATE and intentionally not attached to pathtracer_core.
# A warning policy is a build-time developer concern, never a usage
# requirement: consumers of a library choose their own. pathtracer_core is
# currently an INTERFACE target, and target_link_libraries on an INTERFACE
# target accepts only the INTERFACE keyword - so linking the warnings there
# would force them on every consumer, with no way to say PRIVATE.
target_link_libraries(pathtracer PRIVATE pt::core pt::warnings)

# Installation
# Deliberately minimal: the executable and the public headers, nothing else.
# pathtracer_core is not installed: an INTERFACE library has no artifact.
install(TARGETS pathtracer
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Trailing slash on "include/" is load-bearing: it installs the *contents* of the directory.
install(DIRECTORY include/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  FILES_MATCHING PATTERN "*.hpp"
)
