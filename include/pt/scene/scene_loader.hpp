#pragma once
#include "pt/scene/scene.hpp"
#include <filesystem>
#include <stdexcept>

namespace pt {

class SceneError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;

    ~SceneError() override;
};

/// Loads a scene from the specified file path.
/// Resolves `image.filename` relative to the parent directory of `path`.
/// @throws SceneError If loading or parsing fails.
[[nodiscard]] Scene load_scene(const std::filesystem::path& path);

} // namespace pt
