#pragma once
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
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

// Contract guards: these fail the build if the offset silently loses constexpr or drops a case.
static_assert(offset_ray_origin(Point3(555, 555, 555), Vec3(0, 0, 0)).y() == 555.0_f);
static_assert(offset_ray_origin(Point3(0, 0, 0), Vec3(0, 1, 0)).y() == ray_offset_absolute);
static_assert(offset_ray_origin(Point3(0, 0, 0), Vec3(0, -1, 0)).y() == -ray_offset_absolute);
static_assert(offset_ray_origin(Point3(555, 0, 0), Vec3(1, 0, 0)).x() > 555.0_f);
static_assert(offset_ray_origin(Point3(-555, 0, 0), Vec3(1, 0, 0)).x() > -555.0_f);
static_assert(offset_ray_origin(Point3(555, 0, 0), Vec3(0, 1, 0)).x() == 555.0_f);

} // namespace pt
