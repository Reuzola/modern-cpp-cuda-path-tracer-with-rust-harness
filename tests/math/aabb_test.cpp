#include "pt/math/aabb.hpp"
#include "pt/math/constants.hpp"
#include "pt/math/interval.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include "support/test_support.hpp"
#include <catch2/catch_test_macros.hpp>
#include <optional>

namespace {

using pt::Aabb;
using pt::Float;
using pt::infinity;
using pt::Interval;
using pt::Point3;
using pt::Vec3;
using pt::operator""_f;
using pt_test::require_aabb_near;
using pt_test::require_near;

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

TEST_CASE("intersect accepts tangent hits when rounding collapses slab bounds", "[math][aabb]") {
    // At z = 555, padding (0.0001 / 2) is smaller than ulp(555); distance from origin
    // z = -800 compresses slab thickness to a single ulp, collapsing t_near == t_far in float32.
    const Aabb box(Point3(0.0_f, 0.0_f, 555.0_f), Point3(555.0_f, 555.0_f, 555.0_f));
    const Point3 origin(278.0_f, 278.0_f, -800.0_f);
    const Vec3 inv_dir(infinity, infinity, 1.0_f);
    const Interval ray_t(0.0_f, infinity);

    const auto result = box.intersect(origin, inv_dir, ray_t);

    REQUIRE(result.has_value());

    // Not a precision tolerance: the entry distance is the *padded* near plane,
    // so it sits up to one padding width short of 555's true distance. float
    // collapses it back to exactly 1355 - which is the collapse under test.
    require_near(*result, 1355.0_f, 1e-4);
}
