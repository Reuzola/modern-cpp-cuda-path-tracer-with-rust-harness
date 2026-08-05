# Test-only dependency, scoped here so Catch2Config.cmake's
# CMAKE_MODULE_PATH addition (needed by include(Catch)) stays local.
# Version pinned to 3: v2 exports a same-named target with incompatible
# headers.
find_package(Catch2 3 REQUIRED)

add_executable(pathtracer_tests
  smoke_test.cpp
  sampler_test.cpp
  scene_loader_test.cpp
)

# PRIVATE: an executable has no consumers.
# Catch2::Catch2WithMain (not the WithMain+Catch2 pair vcpkg suggests) -
# WithMain already links Catch2 transitively.
# pt::warnings: applied per-target, easy to forget, keeps -Werror in Debug.
# nlohmann_json deliberately omitted: the loader's public header doesn't
# expose it, so tests don't need to see it.
target_link_libraries(pathtracer_tests PRIVATE
  Catch2::Catch2WithMain
  pt::core
  pt::warnings
)

# Release builds pathtracer_core with ThinLTO, so its archive holds LLVM
# bitcode rather than native objects. A link line without -flto drives the
# system linker with no LTO plugin, and it rejects the archive outright:
# "file format not recognized". Any target linking the core must opt into
# IPO as well - this is a linkability requirement, not a performance choice.
#
# The variables come from the parent scope; add_subdirectory inherits them.
if(PT_ENABLE_IPO AND pt_ipo_supported)
  set_property(TARGET pathtracer_tests PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
endif()

# Baked-in path to scene files. Avoids relying on catch_discover_tests'
# WORKING_DIRECTORY, which would break under `ctest -j`, IDEs, or direct
# binary invocation. Quotes required for a valid C++ string literal.
# PROJECT_SOURCE_DIR keeps the path normalised.
target_compile_definitions(pathtracer_tests PRIVATE
  PT_SCENES_DIR="${PROJECT_SOURCE_DIR}/scenes"
)

# Provided by Catch2 via the CMAKE_MODULE_PATH set above.
include(Catch)

# One CTest entry per Catch2 case (vs. one for the whole binary) for
# process isolation - a segfault/hang only kills its own test, which
# matters for BVH/Monte Carlo tests. Enumeration runs POST_BUILD, so under
# asan-ubsan a sanitizer finding there fails the build, not the tests.
# ADD_TAGS_AS_LABELS exposes Catch2 tags via `ctest -L`.
catch_discover_tests(pathtracer_tests
  ADD_TAGS_AS_LABELS
)