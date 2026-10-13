#pragma once
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include <algorithm>
#include <bit>
#include <cstdint>
#include <limits>
#include <type_traits>

namespace pt {

static_assert(std::numeric_limits<Float>::is_iec559);

// Relies on IEEE-754 sign-magnitude bit layout; guarded by is_iec559 above.
using FloatBits = std::conditional_t<sizeof(Float) == 4, std::int32_t, std::int64_t>;
static_assert(sizeof(FloatBits) == sizeof(Float));

// Ray Tracing Gems ch. 6 (Wächter & Binder) scaling constants.
// Number of ULPs to advance per unit normal component.
inline constexpr Float ray_offset_ulps = 256.0_f;

// Coordinate threshold below which ULPs become too small and precision degrades.
inline constexpr Float ray_offset_origin_threshold = 1.0_f / 32.0_f;

// Fixed world-space offset applied when coordinate magnitude is within origin_threshold.
inline constexpr Float ray_offset_absolute = 1.0_f / 65536.0_f;

/// Offsets ray origin along unit normal `n` using ULP scaling. Requires `n`
/// pointing toward the spawned ray's hemisphere; returns `p` bit-exact if `n` is zero.
[[nodiscard]] constexpr Point3 offset_ray_origin(const Point3& p, const Vec3& n) noexcept {
    Point3 result;

    for (int k = 0; k < 3; ++k) {
        const FloatBits offset_i = static_cast<FloatBits>(ray_offset_ulps * n[k]);
        const FloatBits p_bits = std::bit_cast<FloatBits>(p[k]);
        const Float p_ulp = std::bit_cast<Float>(p[k] < 0 ? p_bits - offset_i : p_bits + offset_i);

        // using (p[k] < 0 ? -p[k] : p[k]) instead of std::fabs because cmath is not constexpr in C++20
        result[k] = (p[k] < 0 ? -p[k] : p[k]) < ray_offset_origin_threshold ? p[k] + ray_offset_absolute * n[k] : p_ulp;
    }

    return result;
}

// Relative error bound for a chain of n floating-point operations (PBRT 3.9.6):
// the exact result lies within a factor of (1 +- gamma(n)) of the computed one.
[[nodiscard]] constexpr Float gamma(int n) noexcept {
    const Float half_eps = std::numeric_limits<Float>::epsilon() / 2;
    const Float n_eps = static_cast<Float>(n) * half_eps;
    return n_eps / (1 - n_eps);
}

// Component-wise magnitude. Hand-rolled because std::fabs is not constexpr in C++20.
[[nodiscard]] constexpr Vec3 component_abs(const Vec3& v) noexcept {
    const Float x = v.x() < 0 ? -v.x() : v.x();
    const Float y = v.y() < 0 ? -v.y() : v.y();
    const Float z = v.z() < 0 ? -v.z() : v.z();
    return Vec3(x, y, z);
}

// The largest coordinate magnitude: the scale that rounding in this vector's arithmetic grows with.
[[nodiscard]] constexpr Float max_abs_component(const Vec3& v) noexcept {
    const Vec3 a = component_abs(v);
    return std::max({a.x(), a.y(), a.z()});
}

// Component-wise bound on |a x b| that ignores cancellation. Each component of a cross product is
// a difference of two products, and its rounding scales with their size, not with the difference.
[[nodiscard]] constexpr Vec3 abs_cross(const Vec3& a, const Vec3& b) noexcept {
    const Vec3 x = component_abs(a);
    const Vec3 y = component_abs(b);
    return Vec3(x.y() * y.z() + x.z() * y.y(), x.z() * y.x() + x.x() * y.z(), x.x() * y.y() + x.y() * y.x());
}

// Contract guards: these fail the build if the offset silently loses constexpr or drops a case.
static_assert(offset_ray_origin(Point3(555, 555, 555), Vec3(0, 0, 0)).y() == 555.0_f);
static_assert(offset_ray_origin(Point3(0, 0, 0), Vec3(0, 1, 0)).y() == ray_offset_absolute);
static_assert(offset_ray_origin(Point3(0, 0, 0), Vec3(0, -1, 0)).y() == -ray_offset_absolute);
static_assert(offset_ray_origin(Point3(555, 0, 0), Vec3(1, 0, 0)).x() > 555.0_f);
static_assert(offset_ray_origin(Point3(-555, 0, 0), Vec3(1, 0, 0)).x() > -555.0_f);
static_assert(offset_ray_origin(Point3(555, 0, 0), Vec3(0, 1, 0)).x() == 555.0_f);
static_assert(max_abs_component(Vec3(-3, 2, 1)) == 3.0_f);

} // namespace pt
