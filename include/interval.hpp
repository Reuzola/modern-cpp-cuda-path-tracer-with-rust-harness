#pragma once
#include "constants.hpp"

class interval {
    public:
        double min{+infinity};
        double max{-infinity};

        constexpr interval() = default;
        constexpr interval(double min, double max) : min(min), max(max) {}

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
};