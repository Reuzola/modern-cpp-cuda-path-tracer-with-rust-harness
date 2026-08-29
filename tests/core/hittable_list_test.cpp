#include "pt/core/hit_record.hpp"
#include "pt/core/hittable.hpp"
#include "pt/core/hittable_list.hpp"
#include "pt/geometry/quad.hpp"
#include "pt/math/aabb.hpp"
#include "pt/math/constants.hpp"
#include "pt/math/interval.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include "support/test_support.hpp"
#include <catch2/catch_test_macros.hpp>

namespace {

using pt::Aabb;
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

// The interval the integrator uses; the lower bound is the shadow-acne epsilon.
const Interval visible{0.001_f, pt::infinity};

/// A hittable that answers at a fixed distance and records how it was asked.
///
/// Deliberately not a real primitive: what is under test is the list's own
/// contract - pick the nearest, narrow the interval, copy the winner's record -
/// and a sphere would add its own intersection maths to every assertion.
class StubHittable final : public Hittable {
public:
    StubHittable(Float t, Float id, const Aabb& box) noexcept : box_(box), t_(t), id_(id) {}

    [[nodiscard]] bool hit(const Ray& r, const Interval& ray_t, HitRecord& rec) const override {
        ++calls_;
        last_ray_t_ = ray_t;

        if (!ray_t.contains(t_)) return false;

        rec.t = t_;
        rec.p = r.at(t_);
        rec.normal = Vec3(0, 0, 1);

        // Stamped into an unrelated field: the list must copy the winner's whole
        // record, not merely its distance.
        rec.u = id_;
        rec.v = id_;

        return true;
    }

    [[nodiscard]] Aabb bounding_box() const override { return box_; }

    [[nodiscard]] int calls() const noexcept { return calls_; }

    [[nodiscard]] const Interval& last_ray_t() const noexcept { return last_ray_t_; }

private:
    Aabb box_;
    Float t_{};
    Float id_{};

    // hit() is const on the interface, so the bookkeeping has to be mutable.
    mutable int calls_ = 0;
    mutable Interval last_ray_t_{};
};

const Ray forward{Point3(0, 0, 0), Vec3(0, 0, 1)};

} // namespace

TEST_CASE("an empty list hits nothing and bounds nothing", "[core][hittable_list]") {
    const HittableList list;

    REQUIRE(list.empty());
    REQUIRE(list.objects().empty());

    HitRecord rec;
    REQUIRE_FALSE(list.hit(forward, visible, rec));

    // The default box is inverted, not zero-sized: min above max on every axis.
    // That is what makes it the identity of the hull below, and it also means an
    // empty list contributes nothing to a parent's bounds.
    const Aabb box = list.bounding_box();
    REQUIRE(box.x.min > box.x.max);
    REQUIRE(box.y.min > box.y.max);
    REQUIRE(box.z.min > box.z.max);
    REQUIRE(box.surface_area() == 0.0_f);
}

TEST_CASE("the single-object constructor is an add", "[core][hittable_list]") {
    const StubHittable object(2.0_f, 1.0_f, Aabb(Point3(-1, -1, -1), Point3(1, 1, 1)));
    const HittableList list(&object);

    REQUIRE_FALSE(list.empty());
    REQUIRE(list.objects().size() == 1);
    REQUIRE(list.objects().front() == &object);

    require_aabb_near(list.bounding_box(), Point3(-1, -1, -1), Point3(1, 1, 1));
}

TEST_CASE("the bounds are the hull of the members", "[core][hittable_list]") {
    const StubHittable left(2.0_f, 1.0_f, Aabb(Point3(-3, -1, -1), Point3(-1, 1, 1)));
    const StubHittable right(3.0_f, 2.0_f, Aabb(Point3(1, -2, -1), Point3(3, 2, 4)));
    const StubHittable nowhere(4.0_f, 3.0_f, Aabb{});

    HittableList list;

    list.add(&left);

    // The first add must reproduce the member's box exactly. It is folded into
    // the default box, so an implementation that took a union of *values* rather
    // than a hull of intervals would leak an infinity into the result here.
    require_aabb_near(list.bounding_box(), Point3(-3, -1, -1), Point3(-1, 1, 1));

    list.add(&right);
    require_aabb_near(list.bounding_box(), Point3(-3, -2, -1), Point3(3, 2, 4));

    // An object with an empty box - an empty group, say - must not swallow the
    // bounds it is added to.
    list.add(&nowhere);
    require_aabb_near(list.bounding_box(), Point3(-3, -2, -1), Point3(3, 2, 4));

    REQUIRE(list.objects().size() == 3);
}

TEST_CASE("clear drops the objects and the bounds together", "[core][hittable_list]") {
    const StubHittable object(2.0_f, 1.0_f, Aabb(Point3(-1, -1, -1), Point3(1, 1, 1)));

    HittableList list;
    list.add(&object);
    REQUIRE_FALSE(list.empty());

    list.clear();

    REQUIRE(list.empty());
    REQUIRE(list.objects().empty());

    // Bounds are cached rather than recomputed on demand, so clearing has to
    // reset them too - a stale box would make a rebuilt list report a region it
    // no longer covers, and the BVH would happily descend into it.
    REQUIRE(list.bounding_box().x.min > list.bounding_box().x.max);
}

TEST_CASE("the nearest object wins, whatever the insertion order", "[core][hittable_list]") {
    const Aabb unit(Point3(-1, -1, -1), Point3(1, 1, 1));

    const StubHittable near_object(2.0_f, 10.0_f, unit);
    const StubHittable far_object(5.0_f, 20.0_f, unit);

    HittableList near_first;
    near_first.add(&near_object);
    near_first.add(&far_object);

    HitRecord rec;
    REQUIRE(near_first.hit(forward, visible, rec));
    require_near(rec.t, 2.0_f);
    require_near(rec.u, 10.0_f);

    HittableList far_first;
    far_first.add(&far_object);
    far_first.add(&near_object);

    HitRecord reordered;
    REQUIRE(far_first.hit(forward, visible, reordered));
    require_near(reordered.t, 2.0_f);
    require_near(reordered.u, 10.0_f);
}

TEST_CASE("every object is asked, with the interval narrowed to the closest hit so far", "[core][hittable_list]") {
    const Aabb unit(Point3(-1, -1, -1), Point3(1, 1, 1));

    const StubHittable near_object(2.0_f, 10.0_f, unit);
    const StubHittable far_object(5.0_f, 20.0_f, unit);

    HittableList list;
    list.add(&near_object);
    list.add(&far_object);

    HitRecord rec;
    REQUIRE(list.hit(forward, visible, rec));

    // The list does not skip anything: it is a linear scan, and the cost it
    // saves is inside the primitives, not in the loop. This is the property the
    // BVH exists to improve on - and the reason bvh_test compares against this
    // class rather than against a second acceleration structure.
    REQUIRE(near_object.calls() == 1);
    REQUIRE(far_object.calls() == 1);

    // The far object was asked with an upper bound of 2, so it rejected itself
    // rather than being rejected afterwards. Lower bound is untouched: only the
    // maximum moves as the scan proceeds.
    require_near(far_object.last_ray_t().min, visible.min);
    require_near(far_object.last_ray_t().max, 2.0_f);
}

TEST_CASE("the caller's interval bounds the search", "[core][hittable_list]") {
    const Aabb unit(Point3(-1, -1, -1), Point3(1, 1, 1));
    const StubHittable object(2.0_f, 10.0_f, unit);

    HittableList list;
    list.add(&object);

    HitRecord rec;

    // Behind the near plane: what a shadow ray leaving a surface must not see.
    REQUIRE_FALSE(list.hit(forward, Interval(3.0_f, pt::infinity), rec));

    // Beyond the far plane: what ConstantMedium relies on when it limits a
    // segment to the distance it already sampled.
    REQUIRE_FALSE(list.hit(forward, Interval(0.001_f, 1.0_f), rec));

    REQUIRE(list.hit(forward, Interval(0.001_f, 2.5_f), rec));
    require_near(rec.t, 2.0_f);
}

TEST_CASE("the winner's whole record survives, and a miss leaves the record alone", "[core][hittable_list]") {
    const Aabb unit(Point3(-1, -1, -1), Point3(1, 1, 1));

    const StubHittable near_object(2.0_f, 10.0_f, unit);
    const StubHittable far_object(5.0_f, 20.0_f, unit);

    HittableList list;
    list.add(&far_object);
    list.add(&near_object);

    HitRecord rec;
    REQUIRE(list.hit(forward, visible, rec));

    // The loop keeps a temporary and assigns the whole record on success. If it
    // wrote fields one at a time, the u/v of the loser would be left behind here
    // and every texture lookup on an occluded surface would be wrong.
    require_near(rec.t, 2.0_f);
    require_near(rec.u, 10.0_f);
    require_near(rec.v, 10.0_f);
    require_vec_near(rec.p, Point3(0, 0, 2));

    // On a miss the caller's record is untouched, which is what lets the
    // integrator keep a record across the surface query and the media loop.
    HitRecord untouched;
    untouched.t = 42.0_f;
    REQUIRE_FALSE(list.hit(forward, Interval(6.0_f, 7.0_f), untouched));
    require_near(untouched.t, 42.0_f);
}

TEST_CASE("at exactly equal distance the last accepting object wins", "[core][hittable_list]") {
    // Two quads covering the same square in z = 2, parameterised in opposite
    // directions, so the same hit point carries different (u, v) and the record
    // says which one answered. The ray is deliberately off-centre: the centre of
    // the square maps to (0.5, 0.5) under both parameterisations and would not
    // tell them apart.
    const Ray offset{Point3(0.5_f, 0.25_f, 0.0_f), Vec3(0, 0, 1)};

    const pt::Quad first(Point3(-1, -1, 2), Vec3(2, 0, 0), Vec3(0, 2, 0), nullptr);
    const pt::Quad second(Point3(1, 1, 2), Vec3(-2, 0, 0), Vec3(0, -2, 0), nullptr);

    HittableList list;
    list.add(&first);
    list.add(&second);

    HitRecord rec;
    REQUIRE(list.hit(offset, visible, rec));
    require_near(rec.t, 2.0_f);

    // Quad's interval test is closed, so the second quad accepts a hit at
    // exactly the closest distance found so far and overwrites the first. The
    // winner is therefore the last one in the list - which is why changing the
    // order primitives are visited in (a rebuilt BVH, a different leaf grouping)
    // changes the image wherever two surfaces are coincident.
    require_near(rec.u, 0.25_f);
    require_near(rec.v, 0.375_f);

    HittableList reversed;
    reversed.add(&second);
    reversed.add(&first);

    HitRecord other;
    REQUIRE(reversed.hit(offset, visible, other));
    require_near(other.u, 0.75_f);
    require_near(other.v, 0.625_f);
}
