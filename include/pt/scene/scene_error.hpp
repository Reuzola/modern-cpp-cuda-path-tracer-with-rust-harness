#pragma once
#include <stdexcept>
#include <string>
#include <string_view>

namespace pt {

// The scene layer's error type, in its own header so that loaders which only
// produce raw data (see obj_loader.hpp) can report errors without pulling in Scene.
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

}
