#pragma once
#include "interval.hpp"
#include "vec3.hpp"
#include <cmath>
#include <iostream>

[[nodiscard]] inline double linear_to_gamma(double linear_component) {
    if (linear_component > 0) return std::sqrt(linear_component);
    return 0.0;
}

[[nodiscard]] inline double remove_nan(double x) { return std::isnan(x) ? 0.0 : x; }

inline void write_color(std::ostream& out, const color& pixel_color) {
    double r = pixel_color.x();
    double g = pixel_color.y();
    double b = pixel_color.z();

    r = remove_nan(r);
    g = remove_nan(g);
    b = remove_nan(b);

    const double r_gamma = linear_to_gamma(r);
    const double g_gamma = linear_to_gamma(g);
    const double b_gamma = linear_to_gamma(b);

    static constexpr interval intensity(0.000, 0.999);

    int r_byte = static_cast<int>(256 * intensity.clamp(r_gamma));
    int g_byte = static_cast<int>(256 * intensity.clamp(g_gamma));
    int b_byte = static_cast<int>(256 * intensity.clamp(b_gamma));

    out << r_byte << ' ' << g_byte << ' ' << b_byte << '\n';
}