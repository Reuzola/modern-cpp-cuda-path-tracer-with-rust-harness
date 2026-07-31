#pragma once
#include "pt/math/scalar.hpp"
#include <cmath>

namespace pt {

class Sampler;

class Vec3 {
public:
    Vec3() : e_{0, 0, 0} {}
    Vec3(Float a, Float b, Float c) : e_{a, b, c} {}

    Float x() const { return e_[0]; }
    Float y() const { return e_[1]; }
    Float z() const { return e_[2]; }

    Vec3 operator-() const { return Vec3(-e_[0], -e_[1], -e_[2]); }

    Float operator[](int i) const { return e_[i]; }
    Float& operator[](int i) { return e_[i]; }

    Vec3& operator+=(const Vec3& v) {
        e_[0] += v.e_[0];
        e_[1] += v.e_[1];
        e_[2] += v.e_[2];
        return *this;
    }

    Vec3& operator*=(Float t) {
        e_[0] *= t;
        e_[1] *= t;
        e_[2] *= t;
        return *this;
    }

    Vec3& operator/=(Float t) {
        return *this *= 1.0_f / t;
    }

    Float length_squared() const {
        return e_[0] * e_[0] + e_[1] * e_[1] + e_[2] * e_[2];
    }

    Float length() const {
        return std::sqrt(length_squared());
    }

    [[nodiscard]] static Vec3 random(Sampler& sampler);

    [[nodiscard]] static Vec3 random(Float min, Float max, Sampler& sampler);

    [[nodiscard]] bool near_zero() const {
        constexpr Float s = 1e-8_f;
        return std::fabs(e_[0]) < s && std::fabs(e_[1]) < s && std::fabs(e_[2]) < s;
    }

private:
    Float e_[3];
};

inline Vec3 operator+(const Vec3& u, const Vec3& v) {
    return Vec3(u.x() + v.x(), u.y() + v.y(), u.z() + v.z());
}

inline Vec3 operator-(const Vec3& u, const Vec3& v) {
    return Vec3(u.x() - v.x(), u.y() - v.y(), u.z() - v.z());
}

inline Vec3 operator*(Float t, const Vec3& v) {
    return Vec3(t * v.x(), t * v.y(), t * v.z());
}

inline Vec3 operator*(const Vec3& v, Float t) {
    return t * v;
}

inline Vec3 operator/(const Vec3& v, Float t) {
    return (1.0_f / t) * v;
}

inline Float dot(const Vec3& u, const Vec3& v) {
    return u.x() * v.x() + u.y() * v.y() + u.z() * v.z();
}

inline Vec3 cross(const Vec3& u, const Vec3& v) {
    return Vec3(
        u.y() * v.z() - u.z() * v.y(),
        u.z() * v.x() - u.x() * v.z(),
        u.x() * v.y() - u.y() * v.x());
}

inline Vec3 unit_vector(const Vec3& v) {
    return v / v.length();
}

[[nodiscard]] inline Vec3 reflect(const Vec3& v, const Vec3& n) {
    return v - 2 * dot(v, n) * n;
}

[[nodiscard]] inline Vec3 refract(const Vec3& uv, const Vec3& n, Float etai_over_etat) {
    Float cos_theta = std::fmin(dot(-uv, n), 1.0_f);

    auto r_out_perp = etai_over_etat * (uv + cos_theta * n);
    auto r_out_parallel = -std::sqrt(std::fabs(1.0_f - r_out_perp.length_squared())) * n;

    return r_out_perp + r_out_parallel;
}

[[nodiscard]] Vec3 random_unit_vector(Sampler& sampler);

[[nodiscard]] Vec3 random_in_unit_disk(Sampler& sampler);

[[nodiscard]] Vec3 random_cosine_direction(Sampler& sampler);

using Point3 = Vec3;

} // namespace pt
