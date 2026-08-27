#include "viewer/camera_controller.hpp"
#include "pt/math/constants.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include "pt/scene/scene.hpp"
#include <algorithm>
#include <cmath>

namespace pt {

namespace {

constexpr Float look_sensitivity = 0.0005_f;
constexpr Float max_pitch = degrees_to_radians(89.0_f);
constexpr Float base_speed_fraction = 0.5_f;
constexpr Float fast_multiplier = 4.0_f;
constexpr Float slow_multiplier = 0.25_f;

} // namespace

CameraController::CameraController(const CameraSettings& settings) : settings_(settings), frame_(settings.vup) {
    const Vec3 look_vec = settings.lookat - settings.lookfrom;
    const Vec3 forward0 = look_vec.near_zero() ? frame_.u() : unit_vector(look_vec);

    const Float a_sin = std::asin(std::clamp(dot(forward0, frame_.w()), -1.0_f, 1.0_f));
    pitch_ = std::clamp(a_sin, -max_pitch, max_pitch);

    yaw_ = std::atan2(dot(forward0, frame_.v()), dot(forward0, frame_.u()));

    const Float len = look_vec.length();
    base_speed_ = (len == 0.0_f ? 1.0_f : len) * base_speed_fraction;
}

bool CameraController::update(const CameraInput& input, Float dt) noexcept {
    if (input.move.near_zero() && input.look_dx == 0.0_f && input.look_dy == 0.0_f) return false;

    yaw_ += input.look_dx * look_sensitivity;
    pitch_ -= input.look_dy * look_sensitivity;
    pitch_ = std::clamp(pitch_, -max_pitch, max_pitch);

    // Wrap yaw so a long session doesn't drift into low angular precision.
    yaw_ = std::remainder(yaw_, 2.0_f * pi);

    const Vec3 f = forward();
    const Vec3 right = unit_vector(cross(f, frame_.w()));
    const Vec3 up = frame_.w();

    const Vec3 move = (input.move.length_squared() > 1.0_f) ? unit_vector(input.move) : input.move;
    Float speed = base_speed_;
    if (input.fast) speed *= fast_multiplier;
    if (input.slow) speed *= slow_multiplier;
    settings_.lookfrom += (move.x() * right + move.y() * up + move.z() * f) * (speed * dt);

    // Only the direction matters: Camera derives its basis from lookfrom - lookat.
    settings_.lookat = settings_.lookfrom + f;
    return true;
}

Vec3 CameraController::forward() const noexcept {
    const Float cos_pitch = std::cos(pitch_);
    return frame_.transform(Vec3(cos_pitch * std::cos(yaw_), cos_pitch * std::sin(yaw_), std::sin(pitch_)));
}

} // namespace pt
