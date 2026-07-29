#pragma once
#include "pt/math/scalar.hpp"

namespace pt {

class color {
public:
    constexpr color() noexcept = default;
    constexpr color(Float r, Float g, Float b) noexcept : r_(r), g_(g), b_(b) {}

    [[nodiscard]] constexpr Float r() const noexcept { return r_; }
    [[nodiscard]] constexpr Float g() const noexcept { return g_; }
    [[nodiscard]] constexpr Float b() const noexcept { return b_; }

    constexpr color& operator+=(const color& c) noexcept {
        r_ += c.r_;
        g_ += c.g_;
        b_ += c.b_;
        return *this;
    }

    [[nodiscard]] static color random();

    [[nodiscard]] static color random(Float min, Float max);

private:
    Float r_{};
    Float g_{};
    Float b_{};
};

[[nodiscard]] constexpr color operator+(const color& u, const color& v) noexcept {
    return color(u.r() + v.r(), u.g() + v.g(), u.b() + v.b());
}

[[nodiscard]] constexpr color operator*(const color& u, const color& v) noexcept {
    return color(u.r() * v.r(), u.g() * v.g(), u.b() * v.b());
}

[[nodiscard]] constexpr color operator*(Float t, const color& c) noexcept {
    return color(t * c.r(), t * c.g(), t * c.b());
}

[[nodiscard]] constexpr color operator*(const color& c, Float t) noexcept {
    return t * c;
}

[[nodiscard]] constexpr color operator/(const color& c, Float t) noexcept {
    const Float inv = 1.0_f / t;
    return inv * c;
}

} // namespace pt
