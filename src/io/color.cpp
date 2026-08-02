#include "pt/io/color.hpp"
#include "pt/math/color.hpp"
#include "pt/math/scalar.hpp"
#include <array>
#include <cassert>
#include <cstdint>

namespace pt {

namespace {

[[nodiscard]] std::uint8_t to_byte(const Float linear) noexcept {
    assert(linear >= 0.0_f && linear < 1.0_f);
    return static_cast<std::uint8_t>(256 * linear);
}

} // namespace

std::array<std::uint8_t, 3> to_ldr_bytes(const Color& c) noexcept {
    return {to_byte(c.r()), to_byte(c.g()), to_byte(c.b())};
}

} // namespace pt
