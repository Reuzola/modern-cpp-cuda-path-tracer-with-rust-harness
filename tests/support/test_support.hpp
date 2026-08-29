#pragma once
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

// Assertion helpers shared by the geometry test suites. Everything here is
// inline: this header is included by more than one translation unit.
namespace pt_test {

inline constexpr double tolerance = 1e-6;

// Catch2's floating-point matchers operate on double. Float may be float, so
// widen explicitly - an implicit promotion would trip -Wdouble-promotion.
[[nodiscard]] inline double widen(pt::Float v) noexcept { return static_cast<double>(v); }

inline void require_vec_near(const pt::Vec3& actual, const pt::Vec3& expected) {
    REQUIRE_THAT(widen(actual.x()), Catch::Matchers::WithinAbs(widen(expected.x()), tolerance));
    REQUIRE_THAT(widen(actual.y()), Catch::Matchers::WithinAbs(widen(expected.y()), tolerance));
    REQUIRE_THAT(widen(actual.z()), Catch::Matchers::WithinAbs(widen(expected.z()), tolerance));
}

inline void require_uv_near(pt::Float actual_u, pt::Float actual_v, pt::Float expected_u, pt::Float expected_v) {
    REQUIRE_THAT(widen(actual_u), Catch::Matchers::WithinAbs(widen(expected_u), tolerance));
    REQUIRE_THAT(widen(actual_v), Catch::Matchers::WithinAbs(widen(expected_v), tolerance));
}

} // namespace pt_test
