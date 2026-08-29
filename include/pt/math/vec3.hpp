#pragma once
#include "pt/math/scalar.hpp"
#include <array>
#include <cmath>
#include <cstddef>
#include <type_traits>

namespace pt {

class Sampler;

class Vec3 {
public:
    constexpr Vec3() noexcept = default;
    constexpr Vec3(Float a, Float b, Float c) noexcept : e_{a, b, c} {}

    [[nodiscard]] constexpr Float x() const noexcept { return e_[0]; }
    [[nodiscard]] constexpr Float y() const noexcept { return e_[1]; }
    [[nodiscard]] constexpr Float z() const noexcept { return e_[2]; }

    [[nodiscard]] constexpr Vec3 operator-() const noexcept { return Vec3(-e_[0], -e_[1], -e_[2]); }

    [[nodiscard]] constexpr Float operator[](int i) const noexcept { return e_[static_cast<std::size_t>(i)]; }
    [[nodiscard]] constexpr Float& operator[](int i) noexcept { return e_[static_cast<std::size_t>(i)]; }

    constexpr Vec3& operator+=(const Vec3& v) noexcept {
        e_[0] += v.e_[0];
        e_[1] += v.e_[1];
        e_[2] += v.e_[2];
        return *this;
    }

    constexpr Vec3& operator*=(Float t) noexcept {
        e_[0] *= t;
        e_[1] *= t;
        e_[2] *= t;
        return *this;
    }

    constexpr Vec3& operator/=(Float t) noexcept {
        return *this *= 1.0_f / t;
    }

    [[nodiscard]] constexpr Float length_squared() const noexcept {
        return e_[0] * e_[0] + e_[1] * e_[1] + e_[2] * e_[2];
    }

    // Not constexpr, here or anywhere else in the engine: <cmath> functions are not constant
    // expressions before C++26, and a hand-rolled compile-time variant would diverge in the last ulp.
    [[nodiscard]] Float length() const noexcept {
        return std::sqrt(length_squared());
    }

    [[nodiscard]] static Vec3 random(Sampler& sampler) noexcept;

    [[nodiscard]] static Vec3 random(Float min, Float max, Sampler& sampler) noexcept;

    [[nodiscard]] bool near_zero() const noexcept {
        constexpr Float s = 1e-8_f;
        return std::fabs(e_[0]) < s && std::fabs(e_[1]) < s && std::fabs(e_[2]) < s;
    }

private:
    std::array<Float, 3> e_{};
};

[[nodiscard]] constexpr Vec3 operator+(const Vec3& u, const Vec3& v) noexcept {
    return Vec3(u.x() + v.x(), u.y() + v.y(), u.z() + v.z());
}

[[nodiscard]] constexpr Vec3 operator-(const Vec3& u, const Vec3& v) noexcept {
    return Vec3(u.x() - v.x(), u.y() - v.y(), u.z() - v.z());
}

[[nodiscard]] constexpr Vec3 operator*(Float t, const Vec3& v) noexcept {
    return Vec3(t * v.x(), t * v.y(), t * v.z());
}

[[nodiscard]] constexpr Vec3 operator*(const Vec3& v, Float t) noexcept {
    return t * v;
}

[[nodiscard]] constexpr Vec3 operator/(const Vec3& v, Float t) noexcept {
    return (1.0_f / t) * v;
}

[[nodiscard]] constexpr Float dot(const Vec3& u, const Vec3& v) noexcept {
    return u.x() * v.x() + u.y() * v.y() + u.z() * v.z();
}

[[nodiscard]] constexpr Vec3 cross(const Vec3& u, const Vec3& v) noexcept {
    return Vec3(
        u.y() * v.z() - u.z() * v.y(),
        u.z() * v.x() - u.x() * v.z(),
        u.x() * v.y() - u.y() * v.x());
}

[[nodiscard]] inline Vec3 unit_vector(const Vec3& v) noexcept {
    return v / v.length();
}

[[nodiscard]] constexpr Vec3 reflect(const Vec3& v, const Vec3& n) noexcept {
    return v - 2 * dot(v, n) * n;
}

[[nodiscard]] inline Vec3 refract(const Vec3& uv, const Vec3& n, Float etai_over_etat) noexcept {
    const Float cos_theta = std::fmin(dot(-uv, n), 1.0_f);

    const Vec3 r_out_perp = etai_over_etat * (uv + cos_theta * n);
    const Vec3 r_out_parallel = -std::sqrt(std::fabs(1.0_f - r_out_perp.length_squared())) * n;

    return r_out_perp + r_out_parallel;
}

[[nodiscard]] Vec3 random_unit_vector(Sampler& sampler) noexcept;

[[nodiscard]] Vec3 random_in_unit_disk(Sampler& sampler) noexcept;

[[nodiscard]] Vec3 random_cosine_direction(Sampler& sampler) noexcept;

using Point3 = Vec3;

// Contract guards: these fail the build if a member below silently loses constexpr.
static_assert(std::is_trivially_copyable_v<Vec3>);
static_assert(Vec3{}.length_squared() == 0.0_f);
static_assert((2.0_f * Vec3(1, 2, 3) + Vec3(0, 0, 2)).z() == 8.0_f);
static_assert(dot(Vec3(1, 2, 3), Vec3(4, 5, 6)) == 32.0_f);
static_assert(cross(Vec3(1, 0, 0), Vec3(0, 1, 0)).z() == 1.0_f);
static_assert(reflect(Vec3(1, -1, 0), Vec3(0, 1, 0)).y() == 1.0_f);
static_assert([] {
    Vec3 v(1, 2, 3);
    v += Vec3(1, 1, 1);
    v *= 2.0_f;
    v[0] = 4.0_f;
    return v[0];
}() == 4.0_f);

} // namespace pt
