#include "pt/core/hit_record.hpp"
#include "pt/core/hittable.hpp"
#include "pt/core/hittable_list.hpp"
#include "pt/geometry/box.hpp"
#include "pt/math/constants.hpp"
#include "pt/math/interval.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include "pt/util/arena.hpp"
#include "support/test_support.hpp"
#include <array>
#include <catch2/catch_test_macros.hpp>

namespace {

using pt::Arena;
using pt::box;
using pt::Float;
using pt::HitRecord;
using pt::Hittable;
using pt::HittableList;
using pt::Interval;
using pt::Point3;
using pt::Ray;
using pt::Vec3;
using pt::operator""_f;
using pt_test::require_aabb_near;
using pt_test::require_near;
using pt_test::require_vec_near;

const Interval visible{0.001_f, pt::infinity};

} // namespace

TEST_CASE("a box is six quads in one list", "[geometry][box]") {
    Arena<Hittable> arena;
    const HittableList* cube = box(arena, Point3(-1, -1, -1), Point3(1, 1, 1), nullptr);

    REQUIRE(cube != nullptr);
    REQUIRE(cube->objects().size() == 6);

    // The faces are flat, so each one pads its own degenerate axis; the hull is
    // a hair larger than the corners asked for. The margin here is wider than
    // that pad and far narrower than a missing face.
    require_aabb_near(cube->bounding_box(), Point3(-1, -1, -1), Point3(1, 1, 1), 1e-3);
}

TEST_CASE("the corners may arrive in any order", "[geometry][box]") {
    Arena<Hittable> arena;
    const HittableList* forward_corners = box(arena, Point3(-1, -2, -3), Point3(1, 2, 3), nullptr);
    const HittableList* swapped_corners = box(arena, Point3(1, 2, 3), Point3(-1, -2, -3), nullptr);

    // The two corners are sorted componentwise before the faces are built, so a
    // scene file may give them in either order. Without that, a swapped pair
    // produces faces with negative extents and a box that is invisible from
    // outside but hits from within.
    require_aabb_near(forward_corners->bounding_box(), Point3(-1, -2, -3), Point3(1, 2, 3), 1e-3);
    require_aabb_near(swapped_corners->bounding_box(), Point3(-1, -2, -3), Point3(1, 2, 3), 1e-3);

    HitRecord from_forward;
    HitRecord from_swapped;
    const Ray probe{Point3(0, 0, -8), Vec3(0, 0, 1)};

    REQUIRE(forward_corners->hit(probe, visible, from_forward));
    REQUIRE(swapped_corners->hit(probe, visible, from_swapped));
    require_near(from_forward.t, from_swapped.t);
}

TEST_CASE("all six faces are where they should be", "[geometry][box]") {
    Arena<Hittable> arena;
    const HittableList* cube = box(arena, Point3(-1, -1, -1), Point3(1, 1, 1), nullptr);

    // One ray per axis direction, each starting 5 units out and aimed at the
    // centre. Every one must stop on its own face at distance 4. A face built
    // from the wrong corner still bounds correctly and still hits from some
    // directions - this is what catches it.
    struct Probe {
        Vec3 from;
        Vec3 direction;
        Point3 expected;
    };

    const std::array probes = {
        Probe{Vec3(-5, 0, 0), Vec3(1, 0, 0), Point3(-1, 0, 0)},
        Probe{Vec3(5, 0, 0), Vec3(-1, 0, 0), Point3(1, 0, 0)},
        Probe{Vec3(0, -5, 0), Vec3(0, 1, 0), Point3(0, -1, 0)},
        Probe{Vec3(0, 5, 0), Vec3(0, -1, 0), Point3(0, 1, 0)},
        Probe{Vec3(0, 0, -5), Vec3(0, 0, 1), Point3(0, 0, -1)},
        Probe{Vec3(0, 0, 5), Vec3(0, 0, -1), Point3(0, 0, 1)},
    };

    for (const Probe& probe : probes) {
        HitRecord rec;
        REQUIRE(cube->hit(Ray(Point3(probe.from), probe.direction), visible, rec));
        require_near(rec.t, 4.0_f);
        require_vec_near(rec.p, probe.expected);

        // Every face is entered from outside, so all six report a front face.
        REQUIRE(rec.front_face);
    }
}

TEST_CASE("each face covers its whole extent", "[geometry][box]") {
    Arena<Hittable> arena;
    const HittableList* cube = box(arena, Point3(-1, -1, -1), Point3(1, 1, 1), nullptr);

    // A grid across one face, out to just inside its edges. A quad built with a
    // right edge vector but the wrong origin leaves a strip of the face missing,
    // which a single centred ray walks straight past.
    constexpr std::array offsets = {-0.95_f, -0.35_f, 0.35_f, 0.95_f};

    for (const Float x : offsets) {
        for (const Float y : offsets) {
            HitRecord rec;
            REQUIRE(cube->hit(Ray(Point3(x, y, -5), Vec3(0, 0, 1)), visible, rec));
            require_near(rec.t, 4.0_f);
        }
    }

    // Just outside the same face: the box ends where it says it does.
    HitRecord rec;
    REQUIRE_FALSE(cube->hit(Ray(Point3(1.05_f, 0, -5), Vec3(0, 0, 1)), visible, rec));
}

TEST_CASE("a ray inside a box leaves through the far face", "[geometry][box]") {
    Arena<Hittable> arena;
    const HittableList* cube = box(arena, Point3(-1, -1, -1), Point3(1, 1, 1), nullptr);

    HitRecord rec;
    REQUIRE(cube->hit(Ray(Point3(0, 0, 0), Vec3(0, 0, 1)), visible, rec));

    // Seen from inside, so the surface is a back face - which is how a Cornell
    // box works at all, and how a participating medium finds its exit point.
    require_near(rec.t, 1.0_f);
    REQUIRE_FALSE(rec.front_face);
    require_vec_near(rec.normal, Vec3(0, 0, -1));
}

TEST_CASE("a flat box degrades to its two real faces", "[geometry][box]") {
    Arena<Hittable> arena;

    // Zero thickness on z: four of the six faces have a zero-length edge, so
    // their normal is not a direction at all and their plane test rejects
    // everything it is asked. Documented rather than guarded - a scene file can
    // ask for this, and the result is a double-sided flat quad rather than a
    // crash or a stray NaN in the image.
    const HittableList* flat = box(arena, Point3(-1, -1, 0), Point3(1, 1, 0), nullptr);
    REQUIRE(flat->objects().size() == 6);

    HitRecord rec;
    REQUIRE(flat->hit(Ray(Point3(0, 0, -5), Vec3(0, 0, 1)), visible, rec));
    require_near(rec.t, 5.0_f);

    REQUIRE_FALSE(flat->hit(Ray(Point3(3, 0, -5), Vec3(0, 0, 1)), visible, rec));
}
