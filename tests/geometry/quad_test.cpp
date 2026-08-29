#include "pt/core/hit_record.hpp"
#include "pt/geometry/quad.hpp"
#include "pt/math/aabb.hpp"
#include "pt/math/constants.hpp"
#include "pt/math/interval.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/sampler.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include "support/test_support.hpp"
#include <catch2/catch_test_macros.hpp>

namespace {

using pt::HitRecord;
using pt::Interval;
using pt::Point3;
using pt::Quad;
using pt::Ray;
using pt::Sampler;
using pt::Vec3;
using pt::operator""_f;
using pt_test::make_sampler;
using pt_test::require_near;
using pt_test::require_uv_near;
using pt_test::require_vec_near;

const Interval visible{0.001_f, pt::infinity};

// A 2x2 square in the plane z = 2, spanning [-1, 1] on both axes. Its normal is
// +z and its area is 4, which keeps the PDF arithmetic below exact.
const Quad square{Point3(-1, -1, 2), Vec3(2, 0, 0), Vec3(0, 2, 0), nullptr};

const Ray forward{Point3(0, 0, 0), Vec3(0, 0, 1)};

} // namespace

TEST_CASE("a ray through the interior reports distance and uv", "[geometry][quad]") {
    HitRecord rec;
    REQUIRE(square.hit(forward, visible, rec));

    require_near(rec.t, 2.0_f);
    require_vec_near(rec.p, Point3(0, 0, 2));

    // uv are the coordinates in the quad's own (u, v) basis, so the centre is
    // (0.5, 0.5) whatever the quad's size or orientation in world space.
    require_uv_near(rec.u, rec.v, 0.5_f, 0.5_f);
}

TEST_CASE("the stored normal turns to face the ray", "[geometry][quad]") {
    HitRecord from_behind;
    REQUIRE(square.hit(forward, visible, from_behind));

    // Travelling along +z into a quad whose normal is +z: the surface is being
    // seen from behind, so the normal is flipped and front_face says so. An
    // emissive quad uses that flag to stay dark on its back side.
    require_vec_near(from_behind.normal, Vec3(0, 0, -1));
    REQUIRE_FALSE(from_behind.front_face);

    HitRecord from_front;
    REQUIRE(square.hit(Ray(Point3(0, 0, 5), Vec3(0, 0, -1)), visible, from_front));
    require_vec_near(from_front.normal, Vec3(0, 0, 1));
    REQUIRE(from_front.front_face);
}

TEST_CASE("the plane is infinite but the quad is not", "[geometry][quad]") {
    HitRecord rec;

    // Both rays meet the plane; only the parameters decide. Getting this wrong
    // is invisible in a closed box and obvious the moment a quad is a light.
    REQUIRE_FALSE(square.hit(Ray(Point3(3, 0, 0), Vec3(0, 0, 1)), visible, rec));
    REQUIRE_FALSE(square.hit(Ray(Point3(0, -4, 0), Vec3(0, 0, 1)), visible, rec));
}

TEST_CASE("the boundary belongs to the quad", "[geometry][quad]") {
    HitRecord rec;

    // Both corners are exactly on the edge of the unit interval and both count as
    // inside, because the containment test is closed. Two quads meeting edge to
    // edge therefore overlap on their shared edge rather than leaving a seam of
    // background pixels between them.
    REQUIRE(square.hit(Ray(Point3(-1, -1, 0), Vec3(0, 0, 1)), visible, rec));
    require_uv_near(rec.u, rec.v, 0.0_f, 0.0_f);

    REQUIRE(square.hit(Ray(Point3(1, 1, 0), Vec3(0, 0, 1)), visible, rec));
    require_uv_near(rec.u, rec.v, 1.0_f, 1.0_f);

    REQUIRE_FALSE(square.hit(Ray(Point3(-1.001_f, 0, 0), Vec3(0, 0, 1)), visible, rec));
}

TEST_CASE("a ray parallel to the plane misses", "[geometry][quad]") {
    HitRecord rec;

    // Inside the plane, pointing along it: the denominator is zero and the
    // distance would be a division by zero. The guard is on the denominator's
    // magnitude, so a grazing ray is dropped rather than turned into an infinity
    // that propagates into rec.p.
    REQUIRE_FALSE(square.hit(Ray(Point3(0, 0, 2), Vec3(1, 0, 0)), visible, rec));
    REQUIRE_FALSE(square.hit(Ray(Point3(0, 0, 0), Vec3(1, 0, 0)), visible, rec));
}

TEST_CASE("the quad's interval test is closed", "[geometry][quad]") {
    HitRecord rec;

    // Accepted at exactly the upper bound, where Sphere rejects. The list narrows
    // its interval to the closest hit so far, so this is precisely why a later
    // quad wins a tie against an earlier one and a later sphere does not.
    REQUIRE(square.hit(forward, Interval(0.001_f, 2.0_f), rec));
    REQUIRE(square.hit(forward, Interval(2.0_f, 3.0_f), rec));
    REQUIRE_FALSE(square.hit(forward, Interval(0.001_f, 1.999_f), rec));
}

TEST_CASE("uv uses the quad's own basis even when it is not orthogonal", "[geometry][quad]") {
    // A sheared parallelogram in z = 0: u along +x, v leaning over it.
    const Quad sheared{Point3(0, 0, 0), Vec3(2, 0, 0), Vec3(1, 2, 0), nullptr};

    // q + 0.5*u + 0.25*v = (1.25, 0.5, 0).
    HitRecord rec;
    REQUIRE(sheared.hit(Ray(Point3(1.25_f, 0.5_f, -3), Vec3(0, 0, 1)), visible, rec));

    require_near(rec.t, 3.0_f);

    // Recovered through w = n / (n . n), which inverts the basis without ever
    // forming a matrix. Projecting onto u and v separately would give the wrong
    // answer here, and would be right for every axis-aligned quad - so a Cornell
    // box would never show the bug.
    require_uv_near(rec.u, rec.v, 0.5_f, 0.25_f);
}

TEST_CASE("the bounds are padded on the flat axis", "[geometry][quad]") {
    const pt::Aabb box = square.bounding_box();

    require_near(box.x.min, -1.0_f);
    require_near(box.x.max, 1.0_f);
    require_near(box.y.min, -1.0_f);
    require_near(box.y.max, 1.0_f);

    // A quad is flat by construction, so its box is empty on one axis. The slab
    // test would then reject every ray, and an axis-aligned wall would be
    // invisible inside a BVH while working fine in a plain list. The pad is
    // asserted as a value rather than as a `>= 1e-4` bound: expand() splits the
    // delta across both sides, so the reconstructed width lands a rounding step
    // below the nominal one.
    require_near(box.z.size(), 0.0001_f, 1e-5);
    require_near((box.z.min + box.z.max) / 2.0_f, 2.0_f);
}

TEST_CASE("pdf_direction converts area to solid angle", "[geometry][quad]") {
    const Point3 origin(0, 0, 0);

    // Straight on, at distance 2, over an area of 4: d^2 / (cos * A) = 4 / 4.
    require_near(square.pdf_direction(origin, Vec3(0, 0, 1)), 1.0_f);

    // The direction's length cancels: t shrinks as the vector grows, and both
    // terms are built from the same vector.
    require_near(square.pdf_direction(origin, Vec3(0, 0, 2)), 1.0_f);

    // Off to the side: further away and seen at a slant, so the same patch of
    // area covers less solid angle and the density rises. Both terms have to be
    // present - dropping the cosine leaves an oblique light too bright.
    require_near(square.pdf_direction(origin, Vec3(1, 0, 2)), 1.3975_f, 1e-3);

    // A direction that misses carries no density at all.
    require_near(square.pdf_direction(origin, Vec3(0, 1, 0)), 0.0_f);
}

TEST_CASE("sample_direction returns the offset to a point on the quad", "[geometry][quad]") {
    const Point3 origin(0, 0, 0);

    // The same stream, consumed by hand and by the quad: this pins the number of
    // draws and their order at once. Swapping the two would move every sampled
    // point to its mirror image across the quad's diagonal.
    Sampler by_hand = make_sampler(11);
    const pt::Float expected_u = by_hand.next_scalar();
    const pt::Float expected_v = by_hand.next_scalar();

    Sampler sampler = make_sampler(11);
    const Vec3 direction = square.sample_direction(origin, sampler);

    HitRecord rec;
    REQUIRE(square.hit(Ray(origin, direction), visible, rec));

    // The returned vector reaches the sampled point exactly, so t is 1. Callers
    // rely on that: the shadow ray is traced with this direction unnormalised.
    require_near(rec.t, 1.0_f);
    require_uv_near(rec.u, rec.v, expected_u, expected_v);

    REQUIRE(sampler.next_uint32() == by_hand.next_uint32());
}

TEST_CASE("every sampled direction has a density", "[geometry][quad]") {
    const Point3 origin(0, 0, 0);
    Sampler sampler = make_sampler(12);

    for (int i = 0; i < 64; ++i) {
        const Vec3 direction = square.sample_direction(origin, sampler);
        REQUIRE(square.pdf_direction(origin, direction) > 0.0_f);
    }
}
