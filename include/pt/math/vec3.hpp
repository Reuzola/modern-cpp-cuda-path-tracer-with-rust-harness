#pragma once
#include "pt/math/scalar.hpp"
#include <cmath>

namespace pt {

class vec3 {
public:
    vec3() : e{0, 0, 0} {}
    vec3(Float a, Float b, Float c) : e{a, b, c} {}

    Float x() const { return e[0]; }
    Float y() const { return e[1]; }
    Float z() const { return e[2]; }

    vec3 operator-() const { return vec3(-e[0], -e[1], -e[2]); }

    Float operator[](int i) const { return e[i]; }
    Float& operator[](int i) { return e[i]; }

    vec3& operator+=(const vec3& v) {
        e[0] += v.e[0];
        e[1] += v.e[1];
        e[2] += v.e[2];
        return *this;
    }

    vec3& operator*=(Float t) {
        e[0] *= t;
        e[1] *= t;
        e[2] *= t;
        return *this;
    }

    vec3& operator/=(Float t) {
        return *this *= 1.0_f / t;
    }

    Float length_squared() const {
        return e[0] * e[0] + e[1] * e[1] + e[2] * e[2];
    }

    Float length() const {
        return std::sqrt(length_squared());
    }

    static vec3 random();

    static vec3 random(Float min, Float max);

    [[nodiscard]] bool near_zero() const {
        constexpr Float s = 1e-8_f;
        return std::fabs(e[0]) < s && std::fabs(e[1]) < s && std::fabs(e[2]) < s;
    }

private:
    Float e[3];
};

inline vec3 operator+(const vec3& u, const vec3& v) {
    return vec3(u.x() + v.x(), u.y() + v.y(), u.z() + v.z());
}

inline vec3 operator-(const vec3& u, const vec3& v) {
    return vec3(u.x() - v.x(), u.y() - v.y(), u.z() - v.z());
}

inline vec3 operator*(Float t, const vec3& v) {
    return vec3(t * v.x(), t * v.y(), t * v.z());
}

inline vec3 operator*(const vec3& v, Float t) {
    return t * v;
}

inline vec3 operator/(const vec3& v, Float t) {
    return (1.0_f / t) * v;
}

inline Float dot(const vec3& u, const vec3& v) {
    return u.x() * v.x() + u.y() * v.y() + u.z() * v.z();
}

inline vec3 cross(const vec3& u, const vec3& v) {
    return vec3(
        u.y() * v.z() - u.z() * v.y(),
        u.z() * v.x() - u.x() * v.z(),
        u.x() * v.y() - u.y() * v.x());
}

inline vec3 unit_vector(const vec3& v) {
    return v / v.length();
}

[[nodiscard]] vec3 random_unit_vector();

[[nodiscard]] inline vec3 reflect(const vec3& v, const vec3& n) {
    return v - 2 * dot(v, n) * n;
}

[[nodiscard]] inline vec3 refract(const vec3& uv, const vec3& n, Float etai_over_etat) {
    Float cos_theta = std::fmin(dot(-uv, n), 1.0_f);

    auto r_out_perp = etai_over_etat * (uv + cos_theta * n);
    auto r_out_parallel = -std::sqrt(std::fabs(1.0_f - r_out_perp.length_squared())) * n;

    return r_out_perp + r_out_parallel;
}

[[nodiscard]] vec3 random_in_unit_disk();

[[nodiscard]] vec3 random_cosine_direction();

using point3 = vec3;

} // namespace pt
