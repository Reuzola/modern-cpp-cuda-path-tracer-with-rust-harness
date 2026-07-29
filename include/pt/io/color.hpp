#pragma once
#include "pt/math/color.hpp"
#include "pt/math/interval.hpp"
#include "pt/math/scalar.hpp"
#include <cmath>
#include <ostream>

namespace pt {

[[nodiscard]] inline Float linear_to_gamma(Float linear_component) {
    if (linear_component > 0) return std::sqrt(linear_component);
    return 0.0_f;
}

[[nodiscard]] inline Float remove_nan(Float x) { return std::isnan(x) ? 0.0_f : x; }

inline void write_color(std::ostream& out, const color& pixel_color) {
    Float r = pixel_color.r();
    Float g = pixel_color.g();
    Float b = pixel_color.b();

    r = remove_nan(r);
    g = remove_nan(g);
    b = remove_nan(b);

    const Float r_gamma = linear_to_gamma(r);
    const Float g_gamma = linear_to_gamma(g);
    const Float b_gamma = linear_to_gamma(b);

    static constexpr interval intensity(0.000_f, 0.999_f);

    int r_byte = static_cast<int>(256 * intensity.clamp(r_gamma));
    int g_byte = static_cast<int>(256 * intensity.clamp(g_gamma));
    int b_byte = static_cast<int>(256 * intensity.clamp(b_gamma));

    out << r_byte << ' ' << g_byte << ' ' << b_byte << '\n';
}

} // namespace pt
