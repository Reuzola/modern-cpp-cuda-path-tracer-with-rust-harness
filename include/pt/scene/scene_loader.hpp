#pragma once
#include "pt/scene/scene.hpp"
#include "pt/scene/scene_error.hpp" // IWYU pragma: export
#include <filesystem>

namespace pt {

/// Loads a scene from the specified file path.
/// Resolves `image.filename` relative to the parent directory of `path`.
/// @throws SceneError If loading or parsing fails.
[[nodiscard]] Scene load_scene(const std::filesystem::path& path);

} // namespace pt
