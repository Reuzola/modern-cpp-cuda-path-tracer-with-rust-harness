#pragma once
#include "vec3.hpp"
#include <iostream>

inline void write_color(std::ostream& out, const color& pixel_color) {
    auto r = pixel_color.x();
    auto g = pixel_color.y();
    auto b = pixel_color.z();

    int r_byte = int(255.999 * r);
    int g_byte = int(255.999 * g);
    int b_byte = int(255.999 * b);

    out << r_byte << ' ' << g_byte << ' ' << b_byte << '\n';
}