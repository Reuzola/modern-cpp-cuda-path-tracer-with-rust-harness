# Scoped so Catch2Config.cmake's CMAKE_MODULE_PATH stays local.
# Pinned to v3: v2's target has incompatible headers.
find_package(Catch2 3 REQUIRED)

add_executable(pathtracer_tests
  smoke/smoke_test.cpp

  util/arena_test.cpp
  util/log_test.cpp
  util/overloaded_test.cpp
  util/stats_test.cpp

  math/sampler_test.cpp
  math/vec3_test.cpp
  math/interval_test.cpp
  math/aabb_test.cpp
  math/onb_test.cpp
  math/transform_test.cpp

  geometry/triangle_test.cpp
  geometry/mesh_test.cpp
  geometry/bvh_test.cpp

  sampling/pdf_test.cpp

  scene/obj_loader_test.cpp
  scene/scene_loader_test.cpp

  render/accumulator_test.cpp
)

# PRIVATE: executables have no consumers.
# WithMain already links Catch2.
# nlohmann_json omitted: not exposed by loader's public header.
target_link_libraries(pathtracer_tests PRIVATE
  Catch2::Catch2WithMain
  pt::core
  pt::warnings
)

# Root of the suite tree: shared headers are addressed from here
# (support/test_support.hpp), never by a bare filename.
target_include_directories(pathtracer_tests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
)

# Core uses ThinLTO in Release; without -flto the linker rejects its
# bitcode archive. Linking targets must opt into IPO too.
if(PT_ENABLE_IPO AND pt_ipo_supported)
  set_property(TARGET pathtracer_tests PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
endif()

# Baked-in paths avoid WORKING_DIRECTORY issues under `ctest -j`/IDEs.
target_compile_definitions(pathtracer_tests PRIVATE
  PT_SCENES_DIR="${PROJECT_SOURCE_DIR}/scenes"
  PT_ASSETS_DIR="${PROJECT_SOURCE_DIR}/assets"
)

# From Catch2 via CMAKE_MODULE_PATH above.
include(Catch)

# Per-case entries isolate crashes; POST_BUILD enumeration surfaces
# sanitizer errors as build failures.
catch_discover_tests(pathtracer_tests
  ADD_TAGS_AS_LABELS
)
