#include "pt/math/aabb.hpp"
#include "pt/math/constants.hpp"
#include "pt/math/interval.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include "support/test_support.hpp"
#include <algorithm>
#include <array>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <limits>
#include <optional>

namespace {

using pt::Aabb;
using pt::Float;
using pt::infinity;
using pt::Interval;
using pt::Point3;
using pt::Vec3;
using pt::operator""_f;
using pt::Sampler;
using pt_test::make_sampler;
using pt_test::require_aabb_near;
using pt_test::require_near;
using pt_test::widen;

constexpr std::array<Float, 3> scales{1.0_f, 555.0_f, 10'000.0_f};

// The same slab test carried out in double: the reference the float path is not
// allowed to undershoot. Under a double build the two coincide and the case below
// only asserts self-consistency; the float build is where it has teeth.
[[nodiscard]] bool reference_hit(const Aabb& box, const Point3& origin, const Vec3& direction) {
    double t_min = 0.0;
    double t_max = std::numeric_limits<double>::infinity();

    for (int axis = 0; axis < 3; ++axis) {
        const double inv = 1.0 / widen(direction[axis]);
        double near = (widen(box.axis_interval(axis).min) - widen(origin[axis])) * inv;
        double far = (widen(box.axis_interval(axis).max) - widen(origin[axis])) * inv;

        if (inv < 0.0) std::swap(near, far);
        t_min = std::max(t_min, near);
        t_max = std::min(t_max, far);

        if (t_max < t_min) return false;
    }
    return true;
}

// The unit cube centred on the origin: every face sits at an exactly
// representable coordinate, which the knife-edge cases below depend on.
const Aabb unit_cube(Point3(-1.0_f, -1.0_f, -1.0_f), Point3(1.0_f, 1.0_f, 1.0_f));

const Interval visible{0.0_f, infinity};

// The reciprocal Bvh::hit computes once per ray. A zero component yields an
// infinity here rather than a division inside the slab loop.
[[nodiscard]] Vec3 reciprocal(const Vec3& direction) noexcept {
    return Vec3(1.0_f / direction.x(), 1.0_f / direction.y(), 1.0_f / direction.z());
}

[[nodiscard]] std::optional<Float> shoot(const Aabb& box, const Point3& origin, const Vec3& direction, const Interval& ray_t = visible) {
    return box.intersect(origin, reciprocal(direction), ray_t);
}

} // namespace

TEST_CASE("the corner constructor sorts its bounds", "[math][aabb]") {
    // Callers hand over two opposite corners in whatever order the geometry
    // produced them; deciding which is the minimum is the box's job.
    const Aabb ordered(Point3(-1.0_f, -2.0_f, -3.0_f), Point3(1.0_f, 2.0_f, 3.0_f));
    const Aabb reversed(Point3(1.0_f, 2.0_f, 3.0_f), Point3(-1.0_f, -2.0_f, -3.0_f));

    require_aabb_near(ordered, Point3(-1.0_f, -2.0_f, -3.0_f), Point3(1.0_f, 2.0_f, 3.0_f));
    require_aabb_near(reversed, Point3(-1.0_f, -2.0_f, -3.0_f), Point3(1.0_f, 2.0_f, 3.0_f));
}

TEST_CASE("a flat axis is padded to a minimum thickness", "[math][aabb]") {
    // Without this, a quad or a planar triangle would own a zero-width slab and
    // the intersection test would reject every ray that is not exactly in its
    // plane. The padding is what makes planar primitives work at all.
    SECTION("a plane gains thickness on its flat axis only") {
        const Aabb plane(Point3(0.0_f, 0.0_f, 5.0_f), Point3(2.0_f, 3.0_f, 5.0_f));

        REQUIRE(plane.z.size() > 0.0_f);
        require_near(plane.z.size(), 0.0001_f);

        // The centre of the padded axis stays where the geometry put it.
        require_near((plane.z.min + plane.z.max) / 2.0_f, 5.0_f, 1.0e-4);

        // Axes that were already thick are untouched.
        require_near(plane.x.size(), 2.0_f);
        require_near(plane.y.size(), 3.0_f);
    }
    SECTION("a point gains thickness on all three") {
        const Aabb point(Point3(1.0_f, 1.0_f, 1.0_f), Point3(1.0_f, 1.0_f, 1.0_f));

        REQUIRE(point.x.size() > 0.0_f);
        REQUIRE(point.y.size() > 0.0_f);
        REQUIRE(point.z.size() > 0.0_f);
    }
    SECTION("the merge constructor deliberately does not pad") {
        // Merging is used to grow a box over many primitives; padding at every
        // step would inflate the result once per merge. The inputs have already
        // been padded by their own constructors.
        const Aabb left(Interval(0.0_f, 1.0_f), Interval(0.0_f, 1.0_f), Interval(0.0_f, 1.0_f));
        const Aabb merged(left, left);

        require_near(merged.x.size(), left.x.size());
    }
}

TEST_CASE("a default-constructed box is empty", "[math][aabb]") {
    const Aabb empty;

    // Every axis is a reversed interval, so the box is the identity element for
    // merging: absorbing it leaves the other operand alone.
    REQUIRE(empty.x.size() < 0.0_f);

    // Clamped at zero rather than left negative: an empty box must not make a
    // SAH split look cheaper than it is.
    require_near(empty.surface_area(), 0.0_f);

    const Aabb merged(empty, unit_cube);
    require_aabb_near(merged, Point3(-1.0_f, -1.0_f, -1.0_f), Point3(1.0_f, 1.0_f, 1.0_f));
}

TEST_CASE("axis_interval indexes the three axes", "[math][aabb]") {
    const Aabb box(Point3(0.0_f, 1.0_f, 2.0_f), Point3(10.0_f, 21.0_f, 32.0_f));

    require_near(box.axis_interval(0).min, box.x.min);
    require_near(box.axis_interval(1).min, box.y.min);
    require_near(box.axis_interval(2).min, box.z.min);

    // Out-of-range indices fall through to x rather than reading past the end.
    // Not a feature to rely on, but the slab loop's bounds depend on it being
    // defined rather than undefined.
    require_near(box.axis_interval(7).min, box.x.min);
}

TEST_CASE("longest_axis breaks ties towards z", "[math][aabb]") {
    SECTION("a clear winner on each axis") {
        REQUIRE(Aabb(Point3(0.0_f, 0.0_f, 0.0_f), Point3(9.0_f, 1.0_f, 2.0_f)).longest_axis() == 0);
        REQUIRE(Aabb(Point3(0.0_f, 0.0_f, 0.0_f), Point3(1.0_f, 9.0_f, 2.0_f)).longest_axis() == 1);
        REQUIRE(Aabb(Point3(0.0_f, 0.0_f, 0.0_f), Point3(1.0_f, 2.0_f, 9.0_f)).longest_axis() == 2);
    }
    SECTION("a cube resolves to z") {
        // The comparisons are strict, so equal extents fall through to the last
        // branch. The SAH builder's own tie-break goes the other way, to the
        // lowest index, and the difference decides the shape of the tree - and
        // with it the golden images. Pinned here so neither drifts alone.
        REQUIRE(unit_cube.longest_axis() == 2);
    }
    SECTION("a tie between the two longest also resolves to z") {
        REQUIRE(Aabb(Point3(0.0_f, 0.0_f, 0.0_f), Point3(9.0_f, 1.0_f, 9.0_f)).longest_axis() == 2);
    }
}

TEST_CASE("surface_area and centroid feed the SAH cost function", "[math][aabb]") {
    SECTION("the area of a box is twice the sum of its three face areas") {
        // 2 * (2*3 + 3*4 + 4*2) = 52.
        const Aabb box(Point3(0.0_f, 0.0_f, 0.0_f), Point3(2.0_f, 3.0_f, 4.0_f));
        require_near(box.surface_area(), 52.0_f);
    }
    SECTION("the centroid is the midpoint of the extents") {
        const Aabb box(Point3(-2.0_f, 0.0_f, 4.0_f), Point3(4.0_f, 6.0_f, 10.0_f));
        require_near(box.centroid().x(), 1.0_f);
        require_near(box.centroid().y(), 3.0_f);
        require_near(box.centroid().z(), 7.0_f);
    }
}

TEST_CASE("translating a box moves it without resizing it", "[math][aabb]") {
    const Vec3 offset(1.0_f, -2.0_f, 3.0_f);

    const Aabb from_left = unit_cube + offset;
    const Aabb from_right = offset + unit_cube;

    require_aabb_near(from_left, Point3(0.0_f, -3.0_f, 2.0_f), Point3(2.0_f, -1.0_f, 4.0_f));
    require_aabb_near(from_right, Point3(0.0_f, -3.0_f, 2.0_f), Point3(2.0_f, -1.0_f, 4.0_f));
    require_near(from_left.surface_area(), unit_cube.surface_area());
}

TEST_CASE("intersect reports the entry distance", "[math][aabb]") {
    SECTION("a ray approaching from outside") {
        const auto t = shoot(unit_cube, Point3(-3.0_f, 0.0_f, 0.0_f), Vec3(1.0_f, 0.0_f, 0.0_f));

        REQUIRE(t.has_value());
        require_near(*t, 2.0_f);
    }
    SECTION("a ray starting inside is clamped to the interval's start") {
        // The traversal orders nodes by this value, so a ray already inside a
        // node must report the interval's start rather than a negative distance
        // that would sort ahead of everything else.
        const auto t = shoot(unit_cube, Point3(0.0_f, 0.0_f, 0.0_f), Vec3(1.0_f, 0.0_f, 0.0_f), Interval(0.5_f, infinity));

        REQUIRE(t.has_value());
        require_near(*t, 0.5_f);
    }
    SECTION("a diagonal ray enters at the far corner's plane") {
        const auto t = shoot(unit_cube, Point3(-3.0_f, -3.0_f, -3.0_f), Vec3(1.0_f, 1.0_f, 1.0_f));

        REQUIRE(t.has_value());
        require_near(*t, 2.0_f);
    }
    SECTION("a ray pointing away from the box misses") {
        REQUIRE_FALSE(shoot(unit_cube, Point3(-3.0_f, 0.0_f, 0.0_f), Vec3(-1.0_f, 0.0_f, 0.0_f)).has_value());
    }
    SECTION("a ray passing beside the box misses") {
        REQUIRE_FALSE(shoot(unit_cube, Point3(-3.0_f, 5.0_f, 0.0_f), Vec3(1.0_f, 0.0_f, 0.0_f)).has_value());
    }
}

TEST_CASE("intersect respects the ray interval", "[math][aabb]") {
    const Point3 origin(-3.0_f, 0.0_f, 0.0_f);
    const Vec3 direction(1.0_f, 0.0_f, 0.0_f);

    SECTION("an interval ending before the box rejects it") {
        REQUIRE_FALSE(shoot(unit_cube, origin, direction, Interval(0.0_f, 1.0_f)).has_value());
    }
    SECTION("an interval starting after the box rejects it") {
        REQUIRE_FALSE(shoot(unit_cube, origin, direction, Interval(9.0_f, infinity)).has_value());
    }
    SECTION("an interval straddling the entry face still accepts") {
        const auto t = shoot(unit_cube, origin, direction, Interval(3.0_f, 5.0_f));

        REQUIRE(t.has_value());
        require_near(*t, 3.0_f);
    }
}

TEST_CASE("intersect handles rays parallel to a slab", "[math][aabb]") {
    // An axis-aligned ray has an infinite reciprocal on its two flat axes, so
    // the slab distances there are plus and minus infinity. Those are ordered
    // by the sign of the reciprocal, never by comparing the two distances,
    // because the next case shows what a comparison would do.
    SECTION("parallel and inside the slab: the other axes decide") {
        const auto t = shoot(unit_cube, Point3(-3.0_f, 0.5_f, -0.5_f), Vec3(1.0_f, 0.0_f, 0.0_f));

        REQUIRE(t.has_value());
        require_near(*t, 2.0_f);
    }
    SECTION("parallel and outside the slab: rejected") {
        REQUIRE_FALSE(shoot(unit_cube, Point3(-3.0_f, 5.0_f, 0.0_f), Vec3(1.0_f, 0.0_f, 0.0_f)).has_value());
    }
    SECTION("a negative direction is ordered by the sign, not by magnitude") {
        const auto t = shoot(unit_cube, Point3(3.0_f, 0.0_f, 0.0_f), Vec3(-1.0_f, 0.0_f, 0.0_f));

        REQUIRE(t.has_value());
        require_near(*t, 2.0_f);
    }
}

TEST_CASE("intersect stays conservative when a slab distance is NaN", "[math][aabb]") {
    // The knife edge: the origin sits exactly on a slab face and the direction
    // is parallel to it, so (face - origin) * infinity evaluates 0 * inf = NaN.
    // NaN loses every comparison, so it survives into t_far untouched, the
    // bound is left alone, and the box stays accepted for the primitive test to
    // settle. A branchless fmin/fmax rewrite propagates the NaN differently and
    // rejects the box, losing a hit the ray really makes - which is the bug
    // this case exists to prevent from coming back.
    SECTION("a ray grazing the +y face along x") {
        const auto t = shoot(unit_cube, Point3(-3.0_f, 1.0_f, 0.0_f), Vec3(1.0_f, 0.0_f, 0.0_f));

        REQUIRE(t.has_value());
        require_near(*t, 2.0_f);
    }
    SECTION("a ray on the edge where two faces meet") {
        const auto t = shoot(unit_cube, Point3(-3.0_f, 1.0_f, -1.0_f), Vec3(1.0_f, 0.0_f, 0.0_f));

        REQUIRE(t.has_value());
        require_near(*t, 2.0_f);
    }
    SECTION("the acceptance is conservative, never a false reject") {
        // A ray on the plane of a face but travelling away from the box must
        // still be rejected: conservative means it may accept too much, not
        // that it accepts everything.
        REQUIRE_FALSE(shoot(unit_cube, Point3(-3.0_f, 1.0_f, 0.0_f), Vec3(-1.0_f, 0.0_f, 0.0_f)).has_value());
    }
}

TEST_CASE("intersect accepts a tangent hit along a box edge", "[math][aabb]") {
    // Grazing the edge x = 0, y = 1: the entry distance from the x slab and the
    // exit distance from the y slab are the same number, exactly. Two things keep
    // the hit - the widened exit distance, and a rejection that compares strictly.
    // The scene-scale collapse this case used to describe is gone: a planar slab
    // is now padded relative to its own coordinate, so it no longer rounds shut.
    const Aabb box(Point3(0, 0, 0), Point3(1, 1, 1));
    const Point3 origin(-1.0_f, 0.0_f, 0.5_f);
    const Vec3 inv_dir(1.0_f, 1.0_f, infinity);

    const auto result = box.intersect(origin, inv_dir, Interval(0.0_f, infinity));

    REQUIRE(result.has_value());
    require_near(*result, 1.0_f);
}

TEST_CASE("the slab test never rejects a box the reference keeps", "[math][aabb]") {
    Sampler sampler = make_sampler(401);

    for (const Float scale : scales) {
        const Aabb box(Point3(scale, scale, scale), Point3(scale + 2, scale + 2, scale + 2));

        for (int i = 0; i < 512; ++i) {
            // Aimed at a point on one of the box's edges: two axes pinned to a
            // face, the third free. There the entry distance from one axis meets
            // the exit distance from another, which is the tie the rounding
            // decides - and where a shrunk interval turns a real hit into the
            // crack you see along the seam between two adjacent boxes.
            const int free_axis = static_cast<int>(sampler.next_below(3));
            Point3 target;
            for (int axis = 0; axis < 3; ++axis) {
                const Interval& ax = box.axis_interval(axis);
                if (axis == free_axis) {
                    target[axis] = sampler.next_scalar(ax.min, ax.max);
                } else {
                    target[axis] = sampler.next_scalar() < 0.5_f ? ax.min : ax.max;
                }
            }

            const Point3 origin = target - 1000.0_f * random_unit_vector(sampler);
            const Vec3 direction = unit_vector(target - origin);
            const Vec3 inv_dir(1.0_f / direction.x(), 1.0_f / direction.y(), 1.0_f / direction.z());

            // A tie may legitimately fall either way in float; what may not happen
            // is the float test being tighter than the exact one.
            if (!reference_hit(box, origin, direction)) {
                continue;
            }

            REQUIRE(box.intersect(origin, inv_dir, Interval(0.0_f, pt::infinity)).has_value());
        }
    }
}

TEST_CASE("a degenerate slab is padded to a resolvable width at any scale", "[math][aabb]") {
    // A quad is planar: one slab has zero width and must be given one. The
    // requirement is not a distance in world units but a distance in ulps of the
    // coordinate, because that is the resolution the traversal arithmetic has
    // there. This is the assertion the old fixed pad fails at both ends.
    for (const Float coordinate : {0.0_f, 0.01_f, 1.0_f, 555.0_f, 10'000.0_f, 1'000'000.0_f}) {
        const Aabb box(Point3(coordinate, coordinate, coordinate), Point3(coordinate + 2, coordinate + 2, coordinate));
        const Float ulp = std::nextafter(coordinate, infinity) - coordinate;

        REQUIRE(box.z.min < box.z.max);
        REQUIRE(box.z.size() >= 4.0_f * ulp);
    }
}

TEST_CASE("padding is a floor on width, not an inflation", "[math][aabb]") {
    // A node's SAH cost is its surface area, so growing a box that is already
    // resolvable is paid for on every traversal that reaches it.
    const Aabb near_origin(Point3(0, 0, 0), Point3(0.01_f, 0.01_f, 0.01_f));
    REQUIRE(near_origin.x.size() == 0.01_f);

    constexpr Float far_away = 1'000'000.0_f;

    // A whole unit of width is resolvable at this coordinate under either scalar
    // type, so it is left exactly as it is.
    const Aabb thick(Point3(far_away, far_away, far_away), Point3(far_away + 1, far_away + 1, far_away + 1));
    REQUIRE(thick.x.size() == 1.0_f);

    // A few ulps of it are not. Which term catches this - the relative one under
    // float, the absolute floor under double - is left to the scalar type on
    // purpose: the two express the same rule at the resolution each one has.
    const Float ulp = std::nextafter(far_away, infinity) - far_away;
    const Float thin_width = 4.0_f * ulp;

    const Aabb thin(Point3(far_away, far_away, far_away),
                    Point3(far_away + thin_width, far_away + thin_width, far_away + thin_width));
    REQUIRE(thin.x.size() > thin_width);
}

TEST_CASE("a padded planar box survives a distant hit", "[math][aabb]") {
    // The case with history: Cornell's back wall is a zero-thickness box at
    // z = 555 and a bounce reaches it from about 1355 units away, where the two
    // slab distances used to round onto each other. The strict statement is in
    // the width case above; this one checks the whole test end to end.
    const Vec3 along_z(infinity, infinity, 1.0_f);

    const Aabb wall(Point3(0, 0, 555), Point3(555, 555, 555));
    const auto hit = wall.intersect(Point3(277.5_f, 277.5_f, -800.0_f), along_z, Interval(0.0_f, infinity));
    REQUIRE(hit.has_value());
    REQUIRE(*hit > 1354.0_f);

    // And at a scale where the fixed pad rounded away completely.
    const Aabb distant(Point3(0, 0, 20'000), Point3(20'000, 20'000, 20'000));
    REQUIRE(distant.intersect(Point3(10'000.0_f, 10'000.0_f, -800.0_f), along_z, Interval(0.0_f, infinity)).has_value());
}
