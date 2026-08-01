#pragma once
#include "pt/math/color.hpp"
#include <array>
#include <cstdint>

namespace pt {

[[nodiscard]] std::array<std::uint8_t, 3> to_ldr_bytes(const Color& c) noexcept;

} // namespace pt
