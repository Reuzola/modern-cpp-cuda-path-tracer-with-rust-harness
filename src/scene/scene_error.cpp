#include "pt/scene/scene_error.hpp"
#include <format>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace pt {

SceneError::SceneError(std::string message) : std::runtime_error(message), message_(std::move(message)), composed_(message_) {}

SceneError::~SceneError() = default;

void SceneError::prepend(std::string_view segment) {
    path_.insert(0, segment);

    if (path_.empty())
        composed_ = message_;
    else
        composed_ = std::format("{}: {}", path_, message_);
}

const std::string& SceneError::location() const noexcept {
    return path_;
}

const char* SceneError::what() const noexcept {
    return composed_.c_str();
}

}
