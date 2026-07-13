#pragma once
#include "random.hpp"
#include <cmath>

class vec3 {
    public:
        vec3() : e{0,0,0} {}
        vec3(double a, double b, double c) : e{a,b,c} {}

        double x() const { return e[0]; }
        double y() const { return e[1]; }
        double z() const { return e[2]; }

        vec3 operator-() const { return vec3(-e[0], -e[1], -e[2]); }

        double  operator[](int i) const { return e[i]; }
        double& operator[](int i) { return e[i]; }

        vec3& operator+=(const vec3& v) {
            e[0] += v.e[0];
            e[1] += v.e[1];
            e[2] += v.e[2];
            return *this;
        }

        vec3& operator*=(double t) {
            e[0] *= t;
            e[1] *= t;
            e[2] *= t;
            return *this;
        }

        vec3& operator/=(double t) {
            return *this *= 1.0/t;
        }

        double length_squared() const {
            return e[0]*e[0] + e[1]*e[1] + e[2]*e[2];
        }

        double length() const {
            return std::sqrt(length_squared());
        }

        static vec3 random() {
            return vec3(random_double(), random_double(), random_double());
        }

        static vec3 random(double min, double max) {
            return vec3(random_double(min, max), random_double(min, max), random_double(min, max));
        }

        [[nodiscard]] bool near_zero() const {
            constexpr auto s = 1e-8;
            return std::fabs(e[0]) < s && std::fabs(e[1]) < s && std::fabs(e[2]) < s;
        }

    private:
        double e[3];
};

inline vec3 operator+(const vec3& u, const vec3& v) {
    return vec3(u.x() + v.x(), u.y() + v.y(), u.z() + v.z());
}

inline vec3 operator-(const vec3& u, const vec3& v) {
    return vec3(u.x() - v.x(), u.y() - v.y(), u.z() - v.z());
}

inline vec3 operator*(const vec3& u, const vec3& v) {
    return vec3(u.x() * v.x(), u.y() * v.y(), u.z() * v.z());
}

inline vec3 operator*(double t, const vec3& v) {
    return vec3(t * v.x(), t * v.y(), t * v.z());
}

inline vec3 operator*(const vec3& v, double t) {
    return t * v;
}

inline vec3 operator/(const vec3& v, double t) {
    return (1.0/t) * v;
}

inline double dot(const vec3& u, const vec3& v) {
    return u.x() * v.x() + u.y() * v.y() + u.z() * v.z();
}

inline vec3 cross(const vec3& u, const vec3& v) {
    return vec3(
        u.y()*v.z() - u.z()*v.y(),
        u.z()*v.x() - u.x()*v.z(),
        u.x()*v.y() - u.y()*v.x()
    );
}

inline vec3 unit_vector(const vec3& v) {
    return v / v.length();
}

[[nodiscard]] inline vec3 random_unit_vector() {
    while (true) {
        auto p = vec3::random(-1, 1);
        auto lensq = p.length_squared();

        if(lensq <= 1 && lensq > 1e-160) return p / std::sqrt(lensq);
    }
}

[[nodiscard]] inline vec3 reflect(const vec3& v, const vec3& n) {
    return v - 2 * dot(v, n) * n;
}

[[nodiscard]] inline vec3 refract(const vec3& uv, const vec3& n, double etai_over_etat) {
    auto cos_theta = std::fmin(dot(-uv, n), 1.0);

    auto r_out_perp = etai_over_etat * (uv + cos_theta * n);
    auto r_out_parallel = -std::sqrt(std::fabs(1.0 - r_out_perp.length_squared())) * n;

    return r_out_perp + r_out_parallel;
}

using point3 = vec3;
using color = vec3;