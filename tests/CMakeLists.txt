# Scoped so Catch2Config.cmake's CMAKE_MODULE_PATH stays local.
# Pinned to v3: v2's target has incompatible headers.
find_package(Catch2 3 REQUIRED)

add_executable(pathtracer_tests
  smoke_test.cpp
  sampler_test.cpp
  scene_loader_test.cpp
  triangle_test.cpp
  mesh_test.cpp
  obj_loader_test.cpp
  bvh_test.cpp
  accumulator_test.cpp
)

# PRIVATE: executables have no consumers.
# WithMain already links Catch2.
# nlohmann_json omitted: not exposed by loader's public header.
target_link_libraries(pathtracer_tests PRIVATE
  Catch2::Catch2WithMain
  pt::core
  pt::warnings
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
