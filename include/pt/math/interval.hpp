#pragma once
#include "pt/math/constants.hpp"
#include "pt/math/scalar.hpp"
#include <algorithm>

namespace pt {

class Interval {
public:
    Float min{+infinity};
    Float max{-infinity};

    constexpr Interval() noexcept = default;
    constexpr Interval(Float lo, Float hi) noexcept : min(lo), max(hi) {}
    constexpr Interval(const Interval& a, const Interval& b) noexcept : min(std::min(a.min, b.min)), max(std::max(a.max, b.max)) {}

    [[nodiscard]] constexpr bool contains(Float x) const noexcept {
        return min <= x && x <= max;
    }

    [[nodiscard]] constexpr bool surrounds(Float x) const noexcept {
        return min < x && x < max;
    }

    [[nodiscard]] constexpr Float clamp(Float x) const noexcept {
        if (x < min) return min;
        if (x > max) return max;
        return x;
    }

    [[nodiscard]] constexpr Float size() const noexcept {
        return max - min;
    }

    [[nodiscard]] constexpr Interval expand(Float delta) const noexcept {
        const Float padding = delta / 2;
        return Interval(min - padding, max + padding);
    }

    static const Interval universe;
};

inline constexpr Interval Interval::universe{-infinity, +infinity};

[[nodiscard]] constexpr Interval operator+(const Interval& i, Float displacement) noexcept {
    return Interval(i.min + displacement, i.max + displacement);
}

[[nodiscard]] constexpr Interval operator+(Float displacement, const Interval& i) noexcept {
    return i + displacement;
}

// Contract guards: these fail the build if a member below silently loses constexpr.
static_assert(Interval::universe.contains(0.0_f));
static_assert(Interval(0.0_f, 4.0_f).clamp(8.0_f) == 4.0_f);
static_assert(Interval(0.0_f, 4.0_f).expand(2.0_f).min == -1.0_f);

} // namespace pt
