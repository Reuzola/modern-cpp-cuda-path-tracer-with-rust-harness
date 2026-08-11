#pragma once
#include "pt/geometry/mesh.hpp"
#include "pt/scene/scene_error.hpp" // IWYU pragma: export
#include <filesystem>

namespace pt {

/// Loads a triangle mesh from a Wavefront OBJ file.
/// Polygons are triangulated. Normals and UVs are kept only if every corner of every face
/// carries them; materials and object groups are ignored, so one file yields one mesh.
/// @throws SceneError If the file cannot be read or contains no triangles.
[[nodiscard]] MeshData load_obj(const std::filesystem::path& path);

} // namespace pt
