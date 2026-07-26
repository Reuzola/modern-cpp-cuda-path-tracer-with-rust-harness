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

# pathtracer_core - the engine
# INTERFACE, not STATIC: the codebase is still header-only, so there is no
# object code to archive. This becomes STATIC once the engine gains its first
# translation unit (src/io/stb_impl.cpp)
add_library(pathtracer_core INTERFACE)
add_library(pt::core ALIAS pathtracer_core)

# BUILD_INTERFACE: these paths are valid while building from this source tree.
# The matching INSTALL_INTERFACE entry is added with the install rules.
target_include_directories(pathtracer_core INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/external>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# Anything linking pathtracer_core is compiled as C++20 or later.
target_compile_features(pathtracer_core INTERFACE cxx_std_20)

# pathtracer - the driver executable
# Deliberately thin: it owns main() and nothing else.
add_executable(pathtracer src/main.cpp)

target_link_libraries(pathtracer PRIVATE pt::core)

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
