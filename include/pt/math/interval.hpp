#pragma once
#include "pt/math/constants.hpp"
#include "pt/math/scalar.hpp"
#include <algorithm>

namespace pt {

class Interval {
public:
    Float min{+infinity};
    Float max{-infinity};

    constexpr Interval() = default;
    constexpr Interval(Float lo, Float hi) : min(lo), max(hi) {}
    constexpr Interval(const Interval& a, const Interval& b) : min(std::min(a.min, b.min)), max(std::max(a.max, b.max)) {}

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

    [[nodiscard]] constexpr Interval expand(Float delta) const {
        const Float padding = delta / 2;
        return Interval(min - padding, max + padding);
    }

    static const Interval universe;
};

inline const Interval Interval::universe{-infinity, +infinity};

[[nodiscard]] constexpr inline Interval operator+(const Interval& i, Float displacement) {
    return Interval(i.min + displacement, i.max + displacement);
}

[[nodiscard]] constexpr inline Interval operator+(Float displacement, const Interval& i) {
    return i + displacement;
}

} // namespace pt
