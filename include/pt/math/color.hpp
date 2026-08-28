#pragma once
#include "pt/math/scalar.hpp"

namespace pt {

class Sampler;

class Color {
public:
    constexpr Color() noexcept = default;
    constexpr Color(Float r, Float g, Float b) noexcept : r_(r), g_(g), b_(b) {}

    [[nodiscard]] constexpr Float r() const noexcept { return r_; }
    [[nodiscard]] constexpr Float g() const noexcept { return g_; }
    [[nodiscard]] constexpr Float b() const noexcept { return b_; }

    constexpr Color& operator+=(const Color& c) noexcept {
        r_ += c.r_;
        g_ += c.g_;
        b_ += c.b_;
        return *this;
    }

    [[nodiscard]] static Color random(Sampler& sampler) noexcept;

    [[nodiscard]] static Color random(Float min, Float max, Sampler& sampler) noexcept;

private:
    Float r_{};
    Float g_{};
    Float b_{};
};

[[nodiscard]] constexpr Color operator+(const Color& u, const Color& v) noexcept {
    return Color(u.r() + v.r(), u.g() + v.g(), u.b() + v.b());
}

[[nodiscard]] constexpr Color operator*(const Color& u, const Color& v) noexcept {
    return Color(u.r() * v.r(), u.g() * v.g(), u.b() * v.b());
}

[[nodiscard]] constexpr Color operator*(Float t, const Color& c) noexcept {
    return Color(t * c.r(), t * c.g(), t * c.b());
}

[[nodiscard]] constexpr Color operator*(const Color& c, Float t) noexcept {
    return t * c;
}

[[nodiscard]] constexpr Color operator/(const Color& c, Float t) noexcept {
    const Float inv = 1.0_f / t;
    return inv * c;
}

} // namespace pt
