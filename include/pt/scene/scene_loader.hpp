#pragma once
#include "pt/scene/scene.hpp"
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>

namespace pt {

class SceneError : public std::runtime_error {
public:
    explicit SceneError(std::string message);

    ~SceneError() override;

    void prepend(std::string_view segment);

    [[nodiscard]] const std::string& location() const noexcept;

    [[nodiscard]] const char* what() const noexcept override;

private:
    std::string message_;
    std::string path_;
    std::string composed_;
};

/// Loads a scene from the specified file path.
/// Resolves `image.filename` relative to the parent directory of `path`.
/// @throws SceneError If loading or parsing fails.
[[nodiscard]] Scene load_scene(const std::filesystem::path& path);

} // namespace pt
