#pragma once
#include <limits>

inline constexpr double infinity = std::numeric_limits<double>::infinity();

inline constexpr double pi = 3.1415926535897932385;

constexpr double degrees_to_radians(double degrees) {
    return degrees * pi / 180.0;
}