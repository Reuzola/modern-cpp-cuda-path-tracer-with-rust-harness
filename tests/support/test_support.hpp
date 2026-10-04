#pragma once
#include "pt/math/aabb.hpp"
#include "pt/math/color.hpp"
#include "pt/math/sampler.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include "pt/render/film.hpp"
#include <bit>
#include <cassert>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cstdint>
#include <format>
#include <optional>
#include <string>
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

// Colour is a distinct type from Vec3 in this engine, so it needs its own
// helper: comparing r/g/b by hand hides which channel drifted.
inline void require_color_near(const pt::Color& actual, const pt::Color& expected, double tol = tolerance) {
    require_near(actual.r(), expected.r(), tol);
    require_near(actual.g(), expected.g(), tol);
    require_near(actual.b(), expected.b(), tol);
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

// Whether `sampler` has taken exactly `expected` draws since it was created by
// make_sampler(stream). Compares the next value against a reference stream
// advanced by hand: the RNG is part of the engine's observable behaviour, so how
// much of it a call consumes is worth stating rather than inferring.
[[nodiscard]] inline bool consumed_draws(pt::Sampler& sampler, std::uint64_t stream, int expected) {
    pt::Sampler reference = make_sampler(stream);
    for (int i = 0; i < expected; ++i) {
        static_cast<void>(reference.next_uint32());
    }
    return sampler.next_uint32() == reference.next_uint32();
}

// The unsigned integer exactly as wide as Float, so a value can be compared by its bits.
using FloatBits = std::conditional_t<sizeof(pt::Float) == sizeof(std::uint64_t), std::uint64_t, std::uint32_t>;
static_assert(sizeof(FloatBits) == sizeof(pt::Float));

// Bits, not ==: == calls +0 and -0 equal, which hides a sign flip, and calls a NaN
// unequal to itself, which would fail a render that reproduces exactly.
[[nodiscard]] inline bool same_bits(pt::Float a, pt::Float b) noexcept {
    return std::bit_cast<FloatBits>(a) == std::bit_cast<FloatBits>(b);
}

[[nodiscard]] inline bool same_bits(const pt::Color& a, const pt::Color& b) noexcept {
    return same_bits(a.r(), b.r()) && same_bits(a.g(), b.g()) && same_bits(a.b(), b.b());
}

// Where two films part ways: how many pixels differ, and the first one in scan order.
struct FilmDifference {
    int differing_pixels{};
    int first_x{};
    int first_y{};
    pt::Color actual;
    pt::Color expected;
};

// Both films must have the same dimensions.
[[nodiscard]] inline std::optional<FilmDifference> film_difference(const pt::Film& actual, const pt::Film& expected) {
    assert(actual.width() == expected.width() && actual.height() == expected.height());

    std::optional<FilmDifference> difference;
    for (int y = 0; y < actual.height(); ++y) {
        for (int x = 0; x < actual.width(); ++x) {
            const pt::Color a = actual.pixel(x, y);
            const pt::Color e = expected.pixel(x, y);
            if (same_bits(a, e)) continue;

            if (!difference) {
                difference = FilmDifference{.differing_pixels = 0, .first_x = x, .first_y = y, .actual = a, .expected = e};
            }
            ++difference->differing_pixels;
        }
    }
    return difference;
}

[[nodiscard]] inline bool bit_identical(const pt::Film& a, const pt::Film& b) {
    return a.width() == b.width() && a.height() == b.height() && !film_difference(a, b);
}

// Hex float is the exact bit pattern in readable form; two values that print the
// same in decimal can still differ in their last bit.
[[nodiscard]] inline std::string hex_color(const pt::Color& c) {
    return std::format("({:a}, {:a}, {:a})", c.r(), c.g(), c.b());
}

// Names the first differing pixel and how many differ; a bare "false" says neither.
inline void require_bit_identical(const pt::Film& actual, const pt::Film& expected) {
    REQUIRE(actual.width() == expected.width());
    REQUIRE(actual.height() == expected.height());

    if (const std::optional<FilmDifference> difference = film_difference(actual, expected)) {
        FAIL(std::format("{} of {} pixels differ, first at ({}, {}): actual {}, expected {}",
                         difference->differing_pixels, actual.width() * actual.height(),
                         difference->first_x, difference->first_y,
                         hex_color(difference->actual), hex_color(difference->expected)));
    }
}

} // namespace pt_test
