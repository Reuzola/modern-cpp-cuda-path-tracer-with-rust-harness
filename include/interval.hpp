#pragma once
#include "constants.hpp"
#include <algorithm>

class interval {
    public:
        double min{+infinity};
        double max{-infinity};

        constexpr interval() = default;
        constexpr interval(double min, double max) : min(min), max(max) {}
        constexpr interval(const interval& a, const interval& b) : min(std::min(a.min, b.min)), max(std::max(a.max, b.max)) {}

        constexpr bool contains(double x) const {
            return min <= x && x <= max;
        }

        constexpr bool surrounds(double x) const {
            return min < x && x < max;
        }

        constexpr double clamp(double x) const {
            if(x < min) return min;
            if(x > max) return max;
            return x;
        }

        constexpr double size() const {
            return max - min;
        }

        [[nodiscard]] constexpr interval expand(double delta) const {
            const double padding = delta / 2;
            return interval(min - padding, max + padding);
        }
};