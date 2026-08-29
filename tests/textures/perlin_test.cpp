#include "pt/math/color.hpp"
#include "pt/math/sampler.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include "pt/textures/noise_texture.hpp"
#include "pt/textures/perlin.hpp"
#include "support/test_support.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cmath>

namespace {

using pt::Color;
using pt::Float;
using pt::NoiseTexture;
using pt::Perlin;
using pt::Point3;
using pt::Sampler;
using pt::operator""_f;
using pt_test::make_sampler;
using pt_test::require_near;

// A spread of positions that is not a lattice: fractional, negative and large
// coordinates all take different paths through the permutation tables.
const Point3 probes[] = {
    Point3(0.13_f, 0.71_f, 0.42_f),
    Point3(3.5_f, -2.25_f, 7.75_f),
    Point3(-9.6_f, 0.05_f, -0.95_f),
    Point3(120.4_f, -33.3_f, 55.55_f),
};

} // namespace

TEST_CASE("the noise field is a function of the seed alone", "[textures][perlin]") {
    Sampler first_stream = make_sampler(51);
    Sampler second_stream = make_sampler(51);

    const Perlin first(first_stream);
    const Perlin second(second_stream);

    // Two fields built from the same seed are the same field. Without this the
    // scene loader could not promise that a given seed reproduces an image, and
    // a Perlin-textured golden would drift every run.
    for (const Point3& p : probes) {
        require_near(first.noise(p), second.noise(p));
    }
}

TEST_CASE("a different seed gives a different field", "[textures][perlin]") {
    Sampler one_stream = make_sampler(52);
    Sampler other_stream = make_sampler(53);

    const Perlin one(one_stream);
    const Perlin other(other_stream);

    int differing = 0;
    for (const Point3& p : probes) {
        if (std::fabs(one.noise(p) - other.noise(p)) > 1e-3_f) ++differing;
    }

    // Guards against a constructor that ignores its sampler - which would still
    // pass every other test in this file.
    REQUIRE(differing >= 3);
}

TEST_CASE("building the field consumes a fixed slice of the stream", "[textures][perlin]") {
    Sampler used = make_sampler(54);
    const Perlin field(used);

    // 256 gradient vectors at three draws each, then three permutations shuffled
    // with 255 draws apiece: 768 + 765. The count is part of the RNG stream, so
    // adding a draw here - a fourth permutation, a rejection loop in the gradient
    // generation - reshuffles every sample that follows and changes the image.
    constexpr int expected_draws = 256 * 3 + 3 * 255;

    Sampler counted = make_sampler(54);
    for (int i = 0; i < expected_draws; ++i) {
        static_cast<void>(counted.next_uint32());
    }

    REQUIRE(used.next_uint32() == counted.next_uint32());
}

TEST_CASE("the field vanishes on the integer lattice", "[textures][perlin]") {
    Sampler stream = make_sampler(55);
    const Perlin field(stream);

    // This is gradient noise, not value noise: the lattice carries directions and
    // the interpolation weights every corner by its offset from the sample point.
    // At a lattice point every offset is zero, so the result is exactly zero, for
    // any seed. A field that produced non-zero values here would be value noise
    // wearing the wrong name - and would show visible blocking at cell corners.
    require_near(field.noise(Point3(0, 0, 0)), 0.0_f);
    require_near(field.noise(Point3(3, -7, 12)), 0.0_f);
    require_near(field.noise(Point3(-256, 256, 1024)), 0.0_f);
}

TEST_CASE("the field stays inside its theoretical bound", "[textures][perlin]") {
    Sampler stream = make_sampler(56);
    const Perlin field(stream);

    Sampler positions = make_sampler(57);
    Float largest{0};

    for (int i = 0; i < 4096; ++i) {
        const Point3 p = Point3::random(-50, 50, positions);
        const Float value = field.noise(p);

        // Not a tolerance check: a NaN fails both comparisons, which is the point.
        // A gradient is a unit vector and the interpolation is a convex
        // combination, so the magnitude cannot exceed sqrt(3)/2.
        REQUIRE(value >= -1.0_f);
        REQUIRE(value <= 1.0_f);

        largest = std::fmax(largest, std::fabs(value));
    }

    // And it does use its range - a field stuck near zero would pass the bound.
    REQUIRE(largest > 0.3_f);
}

TEST_CASE("turbulence sums octaves and never goes negative", "[textures][perlin]") {
    Sampler stream = make_sampler(58);
    const Perlin field(stream);

    const Point3 p(1.3_f, -0.7_f, 2.9_f);

    // No octaves is no signal.
    require_near(field.turb(p, 0), 0.0_f);

    // One octave is the field itself, folded to positive. The absolute value is
    // taken over the sum, not per octave, which is what gives turbulence its
    // creases - the marble veins in the noise scene are those folds.
    require_near(field.turb(p, 1), std::fabs(field.noise(p)));

    // Each further octave halves in weight and doubles in frequency, so the sum
    // converges rather than growing with depth.
    REQUIRE(field.turb(p, 7) >= 0.0_f);
    REQUIRE(field.turb(p, 7) < 2.0_f);

    // The lattice is a zero of every octave at once, since doubling an integer
    // point keeps it on the lattice.
    require_near(field.turb(Point3(2, -3, 5), 7), 0.0_f);
}

TEST_CASE("the noise texture is a grey value in range", "[textures][perlin]") {
    Sampler stream = make_sampler(59);
    const NoiseTexture texture{4.0_f, stream};

    Sampler positions = make_sampler(60);

    for (int i = 0; i < 512; ++i) {
        const Point3 p = Point3::random(-20, 20, positions);
        const Color value = texture.value(0.0_f, 0.0_f, p);

        // A sine folded into [0, 1] and written to all three channels. Staying in
        // range matters beyond looks: an albedo above one is an energy gain, and
        // a path that keeps bouncing off it diverges instead of converging.
        REQUIRE(value.r() >= 0.0_f);
        REQUIRE(value.r() <= 1.0_f);
        require_near(value.g(), value.r());
        require_near(value.b(), value.r());
    }
}

TEST_CASE("the noise texture ignores uv and follows the world position", "[textures][perlin]") {
    Sampler stream = make_sampler(61);
    const NoiseTexture texture{4.0_f, stream};

    const Point3 p(0.4_f, 1.1_f, -2.3_f);

    require_near(texture.value(0.0_f, 0.0_f, p).r(), texture.value(0.9_f, 0.2_f, p).r());
    REQUIRE(std::fabs(texture.value(0, 0, p).r() - texture.value(0, 0, Point3(0.4_f, 1.1_f, -2.9_f)).r()) > 1e-4_f);
}
