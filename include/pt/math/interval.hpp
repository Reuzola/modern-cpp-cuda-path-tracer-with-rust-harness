#pragma once
#include "pt/math/constants.hpp"
#include "pt/math/scalar.hpp"
#include <algorithm>

namespace pt {

class interval {
public:
    Float min{+infinity};
    Float max{-infinity};

    constexpr interval() = default;
    constexpr interval(Float min, Float max) : min(min), max(max) {}
    constexpr interval(const interval& a, const interval& b) : min(std::min(a.min, b.min)), max(std::max(a.max, b.max)) {}

    constexpr bool contains(Float x) const {
        return min <= x && x <= max;
    }

    constexpr bool surrounds(Float x) const {
        return min < x && x < max;
    }

    constexpr Float clamp(Float x) const {
        if (x < min) return min;
        if (x > max) return max;
        return x;
    }

    constexpr Float size() const {
        return max - min;
    }

    [[nodiscard]] constexpr interval expand(Float delta) const {
        const Float padding = delta / 2;
        return interval(min - padding, max + padding);
    }

    static const interval universe;
};

inline const interval interval::universe{-infinity, +infinity};

[[nodiscard]] constexpr inline interval operator+(const interval& i, Float displacement) {
    return interval(i.min + displacement, i.max + displacement);
}

[[nodiscard]] constexpr inline interval operator+(Float displacement, const interval& i) {
    return i + displacement;
}

} // namespace pt
