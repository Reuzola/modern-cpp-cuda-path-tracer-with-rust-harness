#include "pt/math/constants.hpp"
#include "pt/math/robust.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include "support/test_support.hpp"
#include <array>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <limits>

namespace {

using pt::Float;
using pt::infinity;
using pt::offset_ray_origin;
using pt::Point3;
using pt::ray_offset_absolute;
using pt::ray_offset_ulps;
using pt::Vec3;
using pt::operator""_f;

// Steps `value` `count` representable values toward `target`. This is the portable
// spelling of what the integer bit offset does, so the two can be compared bit for
// bit instead of through a tolerance that would hide an off-by-one in the step.
[[nodiscard]] Float advance_ulps(Float value, int count, Float target) {
    for (int i = 0; i < count; i++) {
        value = std::nextafter(value, target);
    }
    return value;
}

// Magnitudes the engine actually renders: a unit-sized prop, a Cornell-scale
// shell, and coordinates far enough out that a fixed world-space epsilon dies.
constexpr std::array<Float, 4> magnitudes{1.0_f, 100.0_f, 10'000.0_f, 1'000'000.0_f};

const int step_count = static_cast<int>(ray_offset_ulps);

} // namespace

TEST_CASE("a zero normal returns the point unchanged", "[math][robust]") {
    // Medium interactions have no surface to escape, so they hand over a zero
    // normal and must get their sample point back bit for bit. This is what lets
    // the offset stay a single unconditional call with no is-surface flag.
    for (const Float magnitude : {0.0_f, 0.001_f, 1.0_f, 555.0_f, 1.0e6_f}) {
        const Point3 p(magnitude, -magnitude, magnitude);
        const Point3 result = offset_ray_origin(p, Vec3(0, 0, 0));

        REQUIRE(result.x() == p.x());
        REQUIRE(result.y() == p.y());
        REQUIRE(result.z() == p.z());
    }
}

TEST_CASE("the offset is exactly an integer ulp step", "[math][robust]") {
    SECTION("on both sides of zero, in both normal directions") {
        // The bit pattern is sign-magnitude: a negative coordinate advances the
        // other way. Getting that branch backwards would push the origin into the
        // surface on every negative-coordinate hit, which is half of most scenes.
        for (const Float magnitude : magnitudes) {
            for (const Float sign : {1.0_f, -1.0_f}) {
                for (const Float normal_x : {1.0_f, -1.0_f}) {
                    const Point3 p(sign * magnitude, 0, 0);
                    const Float result = offset_ray_origin(p, Vec3(normal_x, 0, 0)).x();

                    REQUIRE(result == advance_ulps(p.x(), step_count, normal_x > 0 ? infinity : -infinity));
                }
            }
        }
    }
    SECTION("a fractional normal component takes proportionally fewer steps") {
        // Real hits carry unit normals that are rarely axis aligned, so the
        // per-component step count is the normal's projection, truncated.
        const Point3 p(555.0_f, 0, 0);
        const Float result = offset_ray_origin(p, Vec3(0.5_f, 0, 0)).x();

        REQUIRE(result == advance_ulps(p.x(), step_count / 2, infinity));
    }
    SECTION("axes the normal does not point along are untouched") {
        // A floor quad offsets in y only; any lateral drift would slide the
        // spawn point across the surface it just left.
        const Point3 p(1.0e6_f, 555.0_f, -1.0e6_f);
        const Point3 result = offset_ray_origin(p, Vec3(0, 1, 0));

        REQUIRE(result.x() == p.x());
        REQUIRE(result.z() == p.z());
        REQUIRE(result.y() > p.y());
    }
}

TEST_CASE("the offset grows with the coordinate it protects", "[math][robust]") {
    // The claim this whole helper exists for: the offset is a fixed fraction of
    // the coordinate, because so is the intersection error it has to clear. A
    // fixed world-space epsilon cannot satisfy both ends of this loop at once.
    const double relative_bound = pt_test::widen(ray_offset_ulps) * pt_test::widen(std::numeric_limits<Float>::epsilon());

    Float previous_offset = 0.0_f;
    for (const Float magnitude : magnitudes) {
        const Float offset = offset_ray_origin(Point3(magnitude, 0, 0), Vec3(1, 0, 0)).x() - magnitude;
        const double relative = pt_test::widen(offset) / pt_test::widen(magnitude);

        REQUIRE(offset > previous_offset);
        REQUIRE(relative <= relative_bound);
        REQUIRE(relative > relative_bound / 2.0);

        previous_offset = offset;
    }
}

TEST_CASE("coordinates inside the origin threshold take the absolute offset", "[math][robust]") {
    SECTION("exactly at the origin") {
        // At zero the ulp step is meaningless: it would land in the denormals and
        // move the point by nothing at all.
        const Point3 result = offset_ray_origin(Point3(0, 0, 0), Vec3(0, 1, 0));

        REQUIRE(result.y() == ray_offset_absolute);
        REQUIRE(result.x() == 0.0_f);
        REQUIRE(result.z() == 0.0_f);
    }
    SECTION("the fixed offset dominates the ulp step it replaces") {
        // Near the origin the surface error no longer scales with the coordinate,
        // so the threshold has to hand over to an absolute distance.
        const Float small = 0.01_f;
        const Float result = offset_ray_origin(Point3(small, 0, 0), Vec3(1, 0, 0)).x();

        REQUIRE(result == small + ray_offset_absolute);
        REQUIRE(result > advance_ulps(small, step_count, infinity));
    }
}

TEST_CASE("extreme coordinates stay finite and directed", "[math][robust]") {
    // Adding to the bit pattern walks the exponent as well as the mantissa, so a
    // coordinate near the top of the range must not step into an infinity.
    const Float huge = 1.0e30_f;
    const Point3 result = offset_ray_origin(Point3(huge, -huge, 0), Vec3(1, -1, 0));

    REQUIRE(std::isfinite(result.x()));
    REQUIRE(std::isfinite(result.y()));
    REQUIRE(result.x() > huge);
    REQUIRE(result.y() < -huge);
}
