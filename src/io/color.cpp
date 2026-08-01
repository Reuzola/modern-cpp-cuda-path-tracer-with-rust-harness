#include "pt/io/color.hpp"
#include "pt/math/color.hpp"
#include "pt/math/interval.hpp"
#include "pt/math/scalar.hpp"
#include <array>
#include <cmath>
#include <cstdint>

namespace pt {

namespace {

[[nodiscard]] Float linear_to_gamma(Float linear_component) noexcept {
    return (linear_component > 0) ? std::sqrt(linear_component) : 0.0_f;
}

[[nodiscard]] Float remove_nan(Float x) noexcept {
    return std::isnan(x) ? 0.0_f : x;
}

[[nodiscard]] std::uint8_t to_byte(Float linear) noexcept {
    linear = remove_nan(linear);
    linear = linear_to_gamma(linear);

    static constexpr Interval intensity(0.000_f, 0.999_f);
    return static_cast<std::uint8_t>(256 * intensity.clamp(linear));
}

} // namespace

std::array<std::uint8_t, 3> to_ldr_bytes(const Color& c) noexcept {
    return {to_byte(c.r()), to_byte(c.g()), to_byte(c.b())};
}

} // namespace pt
