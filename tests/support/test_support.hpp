#pragma once
#include "pt/math/aabb.hpp"
#include "pt/math/sampler.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cstdint>
#include <type_traits>

// Assertion helpers shared by the test suites. Everything here is inline: this
// header is included by more than one translation unit.
namespace pt_test {

// Scaled to the engine's scalar type. float carries ~7 decimal digits, so a
// 1e-6 absolute bound is only a few ulps once values reach magnitude 10 and a
// handful of dependent operations have run. A genuine formula error misses by
// orders of magnitude, not by 1e-4, so the looser bound costs no sensitivity.
inline constexpr double tolerance = std::is_same_v<pt::Float, double> ? 1e-6 : 1e-4;

// Catch2's floating-point matchers operate on double. Float may be float, so
// widen explicitly - an implicit promotion would trip -Wdouble-promotion.
[[nodiscard]] inline double widen(pt::Float v) noexcept { return static_cast<double>(v); }

// The reverse of widen. Statistical helpers work in double throughout, so the
// direction handed to the engine has to be narrowed explicitly.
[[nodiscard]] inline pt::Float narrow(double v) noexcept { return static_cast<pt::Float>(v); }

inline void require_near(pt::Float actual, pt::Float expected, double tol = tolerance) {
    REQUIRE_THAT(widen(actual), Catch::Matchers::WithinAbs(widen(expected), tol));
}

inline void require_vec_near(const pt::Vec3& actual, const pt::Vec3& expected, double tol = tolerance) {
    require_near(actual.x(), expected.x(), tol);
    require_near(actual.y(), expected.y(), tol);
    require_near(actual.z(), expected.z(), tol);
}

inline void require_uv_near(pt::Float actual_u, pt::Float actual_v, pt::Float expected_u, pt::Float expected_v, double tol = tolerance) {
    require_near(actual_u, expected_u, tol);
    require_near(actual_v, expected_v, tol);
}

// Compares against the corners a caller would write, not against six loose
// scalars: a transposed axis reads as an obvious failure rather than two.
inline void require_aabb_near(const pt::Aabb& box, const pt::Point3& min_corner, const pt::Point3& max_corner, double tol = tolerance) {
    require_near(box.x.min, min_corner.x(), tol);
    require_near(box.y.min, min_corner.y(), tol);
    require_near(box.z.min, min_corner.z(), tol);
    require_near(box.x.max, max_corner.x(), tol);
    require_near(box.y.max, max_corner.y(), tol);
    require_near(box.z.max, max_corner.z(), tol);
}

// Arbitrary but fixed: tests must not depend on the engine's default seed, and
// a change to that default must not silently reshuffle every statistical test.
inline constexpr std::uint64_t base_seed = 0x5EEDULL;

// `stream` decorrelates one test case from another. Callers pass distinct ids
// so that two cases sharing a file do not consume the same sequence - which
// would let a bug that only shows on certain draws hide in both at once.
[[nodiscard]] inline pt::Sampler make_sampler(std::uint64_t stream) noexcept {
    return pt::Sampler(pt::sampler_seed(base_seed, stream, 0));
}

} // namespace pt_test
