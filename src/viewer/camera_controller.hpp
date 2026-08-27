#pragma once
#include "pt/math/onb.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include "pt/scene/scene.hpp"

namespace pt {

struct CameraInput {
    // Camera-local axes: x = right, y = world up, z = forward. Components in [-1, 1].
    Vec3 move{};
    // Raw cursor delta in pixels; sensitivity is applied by the controller, not the caller.
    Float look_dx{};
    Float look_dy{};
    bool fast{};
    bool slow{};
};

class CameraController final {
public:
    explicit CameraController(const CameraSettings& settings);

    [[nodiscard]] bool update(const CameraInput& input, Float dt) noexcept;

    [[nodiscard]] const CameraSettings& settings() const noexcept { return settings_; }

private:
    CameraSettings settings_;
    Onb frame_; // w() is world up; u()/v() span the plane yaw rotates in.
    Float yaw_{};
    Float pitch_{};
    Float base_speed_{};

    [[nodiscard]] Vec3 forward() const noexcept;
};

} // namespace pt
