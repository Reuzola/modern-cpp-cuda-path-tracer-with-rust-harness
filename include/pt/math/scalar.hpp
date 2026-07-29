#pragma once

namespace pt {

#ifdef PT_FLOAT_AS_DOUBLE
using Float = double;
#else
using Float = float;
#endif

[[nodiscard]] constexpr Float operator""_f(long double v) noexcept {
    return static_cast<Float>(v);
}

} // namespace pt
