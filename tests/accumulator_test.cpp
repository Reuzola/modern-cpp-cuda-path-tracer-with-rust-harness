#include "pt/math/color.hpp"
#include "pt/math/scalar.hpp"
#include "pt/render/accumulator.hpp"
#include "pt/render/film.hpp"
#include <catch2/catch_test_macros.hpp>

using pt::operator""_f;

namespace {

// Color has no operator== by design, so compare component-wise.
// Exact, not approximate: resolve() must reproduce scale * sum bit for bit,
// or every golden image shifts in its last bit.
void require_color_eq(const pt::Color& actual, const pt::Color& expected) {
    REQUIRE(actual.r() == expected.r());
    REQUIRE(actual.g() == expected.g());
    REQUIRE(actual.b() == expected.b());
}

} // namespace

TEST_CASE("Accumulator starts empty", "[render][accumulator]") {
    const pt::Accumulator accumulator(3, 2);

    REQUIRE(accumulator.width() == 3);
    REQUIRE(accumulator.height() == 2);
    REQUIRE(accumulator.sample_count() == 0);

    // The viewer resolves before the first pass completes: no division by zero.
    const pt::Film film = accumulator.resolve();

    REQUIRE(film.width() == 3);
    REQUIRE(film.height() == 2);

    for (int y = 0; y < film.height(); ++y) {
        for (int x = 0; x < film.width(); ++x) {
            require_color_eq(film.pixel(x, y), pt::Color(0.0_f, 0.0_f, 0.0_f));
        }
    }
}

TEST_CASE("Accumulator resolves to the mean of the passes", "[render][accumulator]") {
    pt::Accumulator accumulator(1, 1);

    const pt::Color first(0.1_f, 0.2_f, 0.3_f);
    const pt::Color second(0.4_f, 0.5_f, 0.6_f);
    const pt::Color third(0.7_f, 0.8_f, 0.9_f);

    accumulator.add_sample(0, 0, first);
    accumulator.end_pass();
    accumulator.add_sample(0, 0, second);
    accumulator.end_pass();
    accumulator.add_sample(0, 0, third);
    accumulator.end_pass();

    REQUIRE(accumulator.sample_count() == 3);

    // Mirrors resolve()'s own arithmetic: accumulate in pass order, then
    // multiply by the reciprocal. Writing sum / 3 is a different floating-point
    // operation and may differ in the last bit.
    pt::Color sum(0.0_f, 0.0_f, 0.0_f);
    sum += first;
    sum += second;
    sum += third;

    const pt::Float scale = 1.0_f / 3.0_f;

    require_color_eq(accumulator.resolve().pixel(0, 0), scale * sum);
}

TEST_CASE("Accumulator resolve does not disturb accumulation", "[render][accumulator]") {
    pt::Accumulator accumulator(1, 1);

    const pt::Color first(1.0_f, 0.0_f, 0.0_f);
    const pt::Color second(0.0_f, 1.0_f, 0.0_f);

    accumulator.add_sample(0, 0, first);
    accumulator.end_pass();

    // The viewer resolves every frame while passes are still running.
    const pt::Film after_one = accumulator.resolve();
    require_color_eq(after_one.pixel(0, 0), first);

    accumulator.add_sample(0, 0, second);
    accumulator.end_pass();

    pt::Color sum(0.0_f, 0.0_f, 0.0_f);
    sum += first;
    sum += second;

    const pt::Float scale = 1.0_f / 2.0_f;

    require_color_eq(accumulator.resolve().pixel(0, 0), scale * sum);

    // The earlier Film is an independent copy, not a view.
    require_color_eq(after_one.pixel(0, 0), first);
}

TEST_CASE("Accumulator keeps pixels independent", "[render][accumulator]") {
    pt::Accumulator accumulator(2, 2);

    const pt::Color top_left(0.1_f, 0.0_f, 0.0_f);
    const pt::Color top_right(0.0_f, 0.2_f, 0.0_f);
    const pt::Color bottom_left(0.0_f, 0.0_f, 0.3_f);
    const pt::Color bottom_right(0.4_f, 0.4_f, 0.4_f);

    accumulator.add_sample(0, 0, top_left);
    accumulator.add_sample(1, 0, top_right);
    accumulator.add_sample(0, 1, bottom_left);
    accumulator.add_sample(1, 1, bottom_right);
    accumulator.end_pass();

    const pt::Film film = accumulator.resolve();

    require_color_eq(film.pixel(0, 0), top_left);
    require_color_eq(film.pixel(1, 0), top_right);
    require_color_eq(film.pixel(0, 1), bottom_left);
    require_color_eq(film.pixel(1, 1), bottom_right);
}
