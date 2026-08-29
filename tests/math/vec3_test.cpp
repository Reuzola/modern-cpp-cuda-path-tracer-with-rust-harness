#include "pt/math/constants.hpp"
#include "pt/math/sampler.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include "support/test_support.hpp"
#include <algorithm>
#include <array>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <cstddef>

namespace {

using pt::Float;
using pt::Sampler;
using pt::Vec3;
using pt::operator""_f;
using pt_test::make_sampler;
using pt_test::require_near;
using pt_test::require_vec_near;
using pt_test::widen;

using Catch::Matchers::WithinAbs;

// Large enough that the sample means below are tight, small enough that the
// whole suite stays well inside the time budget even under a sanitizer build.
constexpr int draw_count = 20000;

constexpr double draw_count_d = static_cast<double>(draw_count);

} // namespace

TEST_CASE("length is the square root of length_squared", "[math][vec3]") {
    // A Pythagorean quadruple: 9 + 16 + 144 = 169 is exact in both precisions,
    // so this pins the formula rather than the rounding.
    const Vec3 v(3.0_f, 4.0_f, 12.0_f);

    require_near(v.length_squared(), 169.0_f);
    require_near(v.length(), 13.0_f);

    // No 0/0: the zero vector has a defined length, unlike a normalised one.
    require_near(Vec3().length(), 0.0_f);

    // Scaling by k scales the length by |k|, including a negative k.
    require_near((-2.0_f * v).length(), 26.0_f);
}

TEST_CASE("unit_vector rescales without turning the vector", "[math][vec3]") {
    // Deliberately untested: unit_vector offers no protection against overflow
    // or underflow in length_squared. A vector of magnitude ~1e-20 squares to
    // zero in a float build and the division produces infinities. Every caller
    // in the engine normalises directions of order one, so guarding here would
    // cost a branch on a hot path for a case that cannot arise.
    const Vec3 v(1.0_f, -2.0_f, 3.0_f);
    const Vec3 unit = unit_vector(v);

    require_near(unit.length(), 1.0_f);

    // Same direction, not merely the same length: parallel (cross is zero) and
    // pointing the same way (dot is positive).
    require_vec_near(cross(v, unit), Vec3());
    REQUIRE(dot(v, unit) > 0.0_f);

    SECTION("an already normalised vector is left alone") {
        require_vec_near(unit_vector(unit), unit);
    }
    SECTION("magnitude does not matter") {
        require_near(unit_vector(1.0e12_f * v).length(), 1.0_f);
        require_vec_near(unit_vector(1.0e12_f * v), unit);
    }
}

TEST_CASE("near_zero applies one absolute threshold to all three components", "[math][vec3]") {
    // The threshold is 1e-8, absolute rather than relative: this is a guard for
    // degenerate scatter directions, where the alternative is dividing by a
    // length that rounds to zero.
    SECTION("the zero vector is near zero") {
        REQUIRE(Vec3().near_zero());
    }
    SECTION("the test is on magnitude, so sign is irrelevant") {
        REQUIRE(Vec3(-5.0e-9_f, 5.0e-9_f, -5.0e-9_f).near_zero());
    }
    SECTION("one large component is enough to disqualify the vector") {
        REQUIRE_FALSE(Vec3(5.0e-9_f, 5.0e-9_f, 2.0e-8_f).near_zero());
    }
    SECTION("the comparison is strict, so the threshold itself is not near zero") {
        REQUIRE_FALSE(Vec3(1.0e-8_f, 0.0_f, 0.0_f).near_zero());
    }
}

TEST_CASE("reflect mirrors the vector through the plane of the normal", "[math][vec3]") {
    const Vec3 n(0.0_f, 1.0_f, 0.0_f);
    const Vec3 v = unit_vector(Vec3(1.0_f, -1.0_f, 0.5_f));
    const Vec3 reflected = reflect(v, n);

    // The component along the normal flips; the tangential part is untouched.
    require_near(dot(reflected, n), -dot(v, n));
    require_near(reflected.length(), v.length());

    // A reflection is an involution: applying it twice is the identity.
    require_vec_near(reflect(reflected, n), v);

    SECTION("a vector lying in the plane is unchanged") {
        const Vec3 tangential(1.0_f, 0.0_f, 0.0_f);
        require_vec_near(reflect(tangential, n), tangential);
    }
}

TEST_CASE("refract bends the ray by Snell's law", "[math][vec3]") {
    const Vec3 n(0.0_f, 0.0_f, 1.0_f);

    SECTION("the refracted angle satisfies eta * sin(i) = sin(t)") {
        const Float theta_i = pt::degrees_to_radians(30.0_f);
        const Vec3 uv(std::sin(theta_i), 0.0_f, -std::cos(theta_i));

        // Greater than one: entering the thinner medium, so the ray bends away
        // from the normal and sin(t) > sin(i).
        const Float eta = 1.5_f;
        const Vec3 refracted = refract(uv, n, eta);

        // The perpendicular part carries sin(t) once the result is a unit vector.
        const Float sin_t = std::sqrt(refracted.x() * refracted.x() + refracted.y() * refracted.y());

        require_near(sin_t, eta * std::sin(theta_i));
        require_near(refracted.length(), 1.0_f);

        // The ray keeps going into the surface rather than back out of it.
        REQUIRE(refracted.z() < 0.0_f);
    }
    SECTION("a ratio of one leaves the direction alone") {
        const Vec3 uv = unit_vector(Vec3(0.3_f, 0.2_f, -1.0_f));
        require_vec_near(refract(uv, n, 1.0_f), uv);
    }
    SECTION("normal incidence passes straight through at any ratio") {
        const Vec3 uv(0.0_f, 0.0_f, -1.0_f);
        require_vec_near(refract(uv, n, 1.5_f), uv);
    }
    SECTION("an impossible ratio yields a finite, not a NaN, direction") {
        // sin(t) would exceed one here: physically this is total internal
        // reflection, and it is the caller's job to test for it before asking
        // for a refraction. What refract guarantees is only that the fabs in
        // its square root keeps the result finite instead of poisoning the
        // pixel with NaN.
        const Float theta_i = pt::degrees_to_radians(60.0_f);
        const Vec3 uv(std::sin(theta_i), 0.0_f, -std::cos(theta_i));
        const Vec3 refracted = refract(uv, n, 1.5_f);

        REQUIRE(std::isfinite(widen(refracted.x())));
        REQUIRE(std::isfinite(widen(refracted.y())));
        REQUIRE(std::isfinite(widen(refracted.z())));
    }
}

TEST_CASE("division is a multiplication by the reciprocal", "[math][vec3]") {
    // Exact, not approximate. v * (1/t) and v / t differ in the last bit, and
    // every golden image was produced with the reciprocal form. Switching to a
    // component-wise division would be a silent image change, so the choice is
    // pinned here rather than left to whoever edits the header next.
    const Vec3 v(1.0_f, 2.0_f, 3.0_f);
    const Float t = 3.0_f;
    const Vec3 expected = (1.0_f / t) * v;

    Vec3 compound = v;
    compound /= t;

    REQUIRE(compound.x() == expected.x());
    REQUIRE(compound.y() == expected.y());
    REQUIRE(compound.z() == expected.z());

    REQUIRE((v / t).x() == expected.x());
    REQUIRE((v / t).y() == expected.y());
    REQUIRE((v / t).z() == expected.z());
}

TEST_CASE("Vec3::random consumes the sampler in x, y, z order", "[math][vec3]") {
    // The draw order is part of the image: reordering it reshuffles every
    // sequence downstream and invalidates the golden set.
    SECTION("the unit range overload") {
        Sampler used(make_sampler(1));
        Sampler reference(make_sampler(1));

        const Vec3 v = Vec3::random(used);

        REQUIRE(v.x() == reference.next_scalar());
        REQUIRE(v.y() == reference.next_scalar());
        REQUIRE(v.z() == reference.next_scalar());
    }
    SECTION("the ranged overload") {
        Sampler used(make_sampler(2));
        Sampler reference(make_sampler(2));

        const Vec3 v = Vec3::random(-2.0_f, 5.0_f, used);

        REQUIRE(v.x() == reference.next_scalar(-2.0_f, 5.0_f));
        REQUIRE(v.y() == reference.next_scalar(-2.0_f, 5.0_f));
        REQUIRE(v.z() == reference.next_scalar(-2.0_f, 5.0_f));
    }
}

TEST_CASE("random_unit_vector covers the sphere uniformly", "[math][vec3]") {
    Sampler sampler(make_sampler(3));

    double sum_x = 0.0;
    double sum_y = 0.0;
    double sum_z = 0.0;

    // Rejection sampling has no bound on its iteration count; running it this
    // many times is also the termination test.
    std::array<bool, 8> octant_seen{};

    for (int i = 0; i < draw_count; ++i) {
        const Vec3 v = random_unit_vector(sampler);

        require_near(v.length(), 1.0_f);

        sum_x += widen(v.x());
        sum_y += widen(v.y());
        sum_z += widen(v.z());

        const unsigned int octant =
            (v.x() > 0.0_f ? 1U : 0U) | (v.y() > 0.0_f ? 2U : 0U) | (v.z() > 0.0_f ? 4U : 0U);
        octant_seen[static_cast<std::size_t>(octant)] = true;
    }

    // A uniform direction has zero mean on every axis. The bound is many
    // standard errors wide, so a correct implementation cannot trip it, while
    // any bias towards a pole or a corner of the rejection cube shows up.
    constexpr double mean_bound = 0.02;
    REQUIRE_THAT(sum_x / draw_count_d, WithinAbs(0.0, mean_bound));
    REQUIRE_THAT(sum_y / draw_count_d, WithinAbs(0.0, mean_bound));
    REQUIRE_THAT(sum_z / draw_count_d, WithinAbs(0.0, mean_bound));

    REQUIRE(std::ranges::all_of(octant_seen, [](bool seen) { return seen; }));
}

TEST_CASE("random_in_unit_disk stays inside the z = 0 disk", "[math][vec3]") {
    Sampler sampler(make_sampler(4));

    double sum_x = 0.0;
    double sum_y = 0.0;

    for (int i = 0; i < draw_count; ++i) {
        const Vec3 p = random_in_unit_disk(sampler);

        // Exact: the disk lies in a plane, so z is set rather than computed.
        // Defocus blur offsets the camera origin along its own basis vectors,
        // and a non-zero z here would push the lens sample off the aperture.
        REQUIRE(p.z() == 0.0_f);
        REQUIRE(p.length_squared() < 1.0_f);

        sum_x += widen(p.x());
        sum_y += widen(p.y());
    }

    constexpr double mean_bound = 0.02;
    REQUIRE_THAT(sum_x / draw_count_d, WithinAbs(0.0, mean_bound));
    REQUIRE_THAT(sum_y / draw_count_d, WithinAbs(0.0, mean_bound));
}

TEST_CASE("random_cosine_direction is cosine-weighted about +z", "[math][vec3]") {
    Sampler sampler(make_sampler(5));

    double sum_z = 0.0;

    for (int i = 0; i < draw_count; ++i) {
        const Vec3 v = random_cosine_direction(sampler);

        // The local frame's hemisphere is the one around +z; CosinePdf rotates
        // it onto the surface normal afterwards.
        REQUIRE(v.z() >= 0.0_f);
        require_near(v.length(), 1.0_f);

        sum_z += widen(v.z());
    }

    // The signature of the distribution: E[cos] is 2/3 for a cosine-weighted
    // hemisphere and 1/2 for a uniform one, so this bound tells the two apart
    // rather than merely confirming that the directions point upwards.
    REQUIRE_THAT(sum_z / draw_count_d, WithinAbs(2.0 / 3.0, 0.01));
}
