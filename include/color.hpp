#pragma once
#include "interval.hpp"
#include "vec3.hpp"
#include <iostream>

inline void write_color(std::ostream& out, const color& pixel_color) {
    const auto r = pixel_color.x();
    const auto g = pixel_color.y();
    const auto b = pixel_color.z();

    static constexpr interval intensity(0.000, 0.999);

    int r_byte = static_cast<int>(256 * intensity.clamp(r));
    int g_byte = static_cast<int>(256 * intensity.clamp(g));
    int b_byte = static_cast<int>(256 * intensity.clamp(b));

    out << r_byte << ' ' << g_byte << ' ' << b_byte << '\n';
}