#pragma once
#include "pt/math/scalar.hpp"
#include <limits>
#include <numbers>

namespace pt {

inline constexpr Float infinity = std::numeric_limits<Float>::infinity();

inline constexpr Float pi = std::numbers::pi_v<Float>;

[[nodiscard]] constexpr Float degrees_to_radians(Float degrees) {
    return degrees * pi / 180.0_f;
}

} // namespace pt
