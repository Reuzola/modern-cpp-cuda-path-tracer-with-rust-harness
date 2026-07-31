#include "pt/math/sampler.hpp"
#include "pt/math/scalar.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <type_traits>

using pt::operator""_f;

namespace {

constexpr std::uint64_t test_seed = pt::sampler_seed(0, 0, 0);

constexpr int draw_count = 100000;

} // namespace

TEST_CASE("a sampler is a cheap, copyable value", "[sampler]") {
    STATIC_REQUIRE(std::is_trivially_copyable_v<pt::Sampler>);
    STATIC_REQUIRE(sizeof(pt::Sampler) == sizeof(std::uint64_t));
}

TEST_CASE("the same seed replays the same sequence", "[sampler]") {
    pt::Sampler first(test_seed);
    pt::Sampler second(test_seed);

    bool identical = true;
    for (int i = 0; i < 1000; i++) {
        identical = identical && (first.next_uint32() == second.next_uint32());
    }

    REQUIRE(identical);
}

TEST_CASE("neighbouring samples and pixels get unrelated streams", "[sampler]") {
    pt::Sampler pixel_a(pt::sampler_seed(0, 0, 0));
    pt::Sampler next_sample(pt::sampler_seed(0, 0, 1));
    pt::Sampler next_pixel(pt::sampler_seed(0, 1, 0));

    const std::uint32_t reference = pixel_a.next_uint32();
    REQUIRE(reference != next_sample.next_uint32());
    REQUIRE(reference != next_pixel.next_uint32());

    const std::uint64_t difference = pt::sampler_seed(0, 100, 0) ^ pt::sampler_seed(0, 101, 0);
    REQUIRE(std::popcount(difference) > 20);
}

TEST_CASE("seed derivation is a compile-time pure function", "[sampler]") {
    STATIC_REQUIRE(pt::sampler_seed(1, 2, 3) == pt::sampler_seed(1, 2, 3));
    STATIC_REQUIRE(pt::sampler_seed(1, 2, 3) != pt::sampler_seed(1, 2, 4));
    STATIC_REQUIRE(pt::sampler_seed(1, 2, 3) != pt::sampler_seed(1, 3, 3));
    STATIC_REQUIRE(pt::sampler_seed(1, 2, 3) != pt::sampler_seed(2, 2, 3));
}

TEST_CASE("next_scalar stays in [0, 1)", "[sampler]") {
    pt::Sampler sampler(test_seed);

    pt::Float lowest = 1.0_f;
    pt::Float highest = 0.0_f;
    for (int i = 0; i < draw_count; i++) {
        const pt::Float value = sampler.next_scalar();
        lowest = std::min(lowest, value);
        highest = std::max(highest, value);
    }

    REQUIRE(lowest >= 0.0_f);
    REQUIRE(highest < 1.0_f);
}

TEST_CASE("next_scalar_positive stays in (0, 1]", "[sampler]") {
    pt::Sampler sampler(test_seed);

    pt::Float lowest = 1.0_f;
    pt::Float highest = 0.0_f;
    for (int i = 0; i < draw_count; i++) {
        const pt::Float value = sampler.next_scalar_positive();
        lowest = std::min(lowest, value);
        highest = std::max(highest, value);
    }

    REQUIRE(lowest > 0.0_f);
    REQUIRE(highest <= 1.0_f);
}

TEST_CASE("the ranged overload stays inside its bounds", "[sampler]") {
    pt::Sampler sampler(test_seed);

    pt::Float lowest = 5.0_f;
    pt::Float highest = -2.0_f;
    for (int i = 0; i < draw_count; i++) {
        const pt::Float value = sampler.next_scalar(-2.0_f, 5.0_f);
        lowest = std::min(lowest, value);
        highest = std::max(highest, value);
    }

    REQUIRE(lowest >= -2.0_f);
    REQUIRE(highest < 5.0_f);
}

TEST_CASE("next_below never reaches its bound", "[sampler]") {
    pt::Sampler sampler(test_seed);

    std::array<int, 4> counts{};
    bool in_range = true;
    for (int i = 0; i < draw_count; i++) {
        const std::uint32_t value = sampler.next_below(4);
        if (value >= counts.size()) {
            in_range = false;
        } else {
            counts[value]++;
        }
    }

    REQUIRE(in_range);
    REQUIRE(*std::ranges::min_element(counts) > draw_count / 8);
}

TEST_CASE("integer draws handle degenerate and negative ranges", "[sampler]") {
    pt::Sampler sampler(test_seed);

    REQUIRE(sampler.next_below(1) == 0);
    REQUIRE(sampler.next_int(5, 5) == 5);

    int lowest = 3;
    int highest = -3;
    for (int i = 0; i < draw_count; i++) {
        const int value = sampler.next_int(-3, 3);
        lowest = std::min(lowest, value);
        highest = std::max(highest, value);
    }

    REQUIRE(lowest == -3);
    REQUIRE(highest == 3);
}

TEST_CASE("the generator has not silently changed", "[sampler]") {
    pt::Sampler sampler(test_seed);

    REQUIRE(sampler.next_uint32() == 2919187349U);
    REQUIRE(sampler.next_uint32() == 2869434537U);
    REQUIRE(sampler.next_uint32() == 576170658U);
    REQUIRE(sampler.next_uint32() == 1112811159U);
}
