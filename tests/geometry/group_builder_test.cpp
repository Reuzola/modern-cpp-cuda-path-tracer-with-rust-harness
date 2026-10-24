#include "pt/core/hit_record.hpp"
#include "pt/core/hittable.hpp"
#include "pt/core/hittable_list.hpp"
#include "pt/geometry/box.hpp"
#include "pt/geometry/bvh.hpp"
#include "pt/geometry/group_builder.hpp"
#include "pt/geometry/instance.hpp"
#include "pt/geometry/primitive_kind.hpp"
#include "pt/geometry/quad.hpp"
#include "pt/geometry/sphere.hpp"
#include "pt/materials/lambertian.hpp"
#include "pt/materials/material.hpp"
#include "pt/math/aabb.hpp"
#include "pt/math/color.hpp"
#include "pt/math/constants.hpp"
#include "pt/math/interval.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/sampler.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/transform.hpp"
#include "pt/math/vec3.hpp"
#include "pt/textures/solid_color.hpp"
#include "pt/util/arena.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace {

using pt::Aabb;
using pt::BoxFaces;
using pt::Float;
using pt::GeometryRef;
using pt::GroupBuilder;
using pt::HitRecord;
using pt::Hittable;
using pt::HittableList;
using pt::Interval;
using pt::Point3;
using pt::PrimitiveKind;
using pt::Quad;
using pt::Ray;
using pt::Sampler;
using pt::Sphere;
using pt::Transform;
using pt::Vec3;
using pt::operator""_f;

const Interval visible{0.0_f, pt::infinity};
const Ray forward{Point3(0, 0, 0), Vec3(0, 0, 1)};

const pt::SolidColor albedo{pt::Color(0.5_f, 0.5_f, 0.5_f)};
const pt::Lambertian shared_material{&albedo};

// A container's worth of children, each kept twice: as the typed reference a builder receives, and as
// a Hittable in the list the reference scans. One material per object, so a record names its object.
// Immovable: the materials point at `albedo_`.
class Children {
public:
    Children() = default;
    Children(const Children&) = delete;
    Children& operator=(const Children&) = delete;

    void add_sphere(const Point3& center, Float radius) {
        const Sphere* sphere = objects_.create<Sphere>(center, radius, new_material());
        refs_.emplace_back(sphere);
        list_.add(sphere);
    }

    void add_quad(const Point3& q, const Vec3& u, const Vec3& v) {
        const Quad* quad = objects_.create<Quad>(q, u, v, new_material());
        refs_.emplace_back(quad);
        list_.add(quad);
    }

    // A sphere behind a transform. An Instance is no primitive of any kind, so the builder keeps it opaque.
    void add_moved_sphere(const Point3& center, Float radius, const Vec3& offset) {
        const Sphere* child = objects_.create<Sphere>(center, radius, new_material());
        const pt::Instance* moved = objects_.create<pt::Instance>(child, Transform::translation(offset));
        refs_.emplace_back(moved);
        list_.add(moved);
    }

    [[nodiscard]] const std::vector<GeometryRef>& refs() const noexcept { return refs_; }

    [[nodiscard]] const HittableList& list() const noexcept { return list_; }

private:
    [[nodiscard]] const pt::Material* new_material() { return materials_.create<pt::Lambertian>(&albedo_); }

    const pt::SolidColor albedo_{pt::Color(0.5_f, 0.5_f, 0.5_f)};
    pt::Arena<pt::Material> materials_;
    pt::Arena<Hittable> objects_;
    std::vector<GeometryRef> refs_;
    HittableList list_;
};

// Braced, so the three draws happen left to right whatever the compiler.
[[nodiscard]] Point3 point_in(Sampler& sampler, Float lo, Float hi) {
    return Point3{sampler.next_scalar(lo, hi), sampler.next_scalar(lo, hi), sampler.next_scalar(lo, hi)};
}

[[nodiscard]] Point3 point_in(Sampler& sampler, const Aabb& bounds) {
    return Point3{sampler.next_scalar(bounds.x.min, bounds.x.max), sampler.next_scalar(bounds.y.min, bounds.y.max),
                  sampler.next_scalar(bounds.z.min, bounds.z.max)};
}

// All three kinds of child, interleaved, around a moved sphere at the middle of the cloud.
void fill_children(Children& children, std::uint64_t seed) {
    Sampler sampler{pt::sampler_seed(seed, 0, 0)};

    for (int i = 0; i < 40; ++i) {
        const Point3 center = point_in(sampler, -10.0_f, 10.0_f);
        const Float radius = sampler.next_scalar(0.3_f, 1.2_f);
        children.add_sphere(center, radius);

        const Point3 corner = point_in(sampler, -10.0_f, 10.0_f);
        const Vec3 u = Vec3::random(-3.0_f, 3.0_f, sampler);
        const Vec3 v = Vec3::random(-3.0_f, 3.0_f, sampler);
        children.add_quad(corner, u, v);
    }

    children.add_moved_sphere(Point3(0, 0, 0), 2.0_f, Vec3(1.0_f, -2.0_f, 0.5_f));
}

[[nodiscard]] Ray ray_into(Sampler& sampler, const Aabb& bounds) {
    const Float radius = 0.5_f * Vec3(bounds.x.size(), bounds.y.size(), bounds.z.size()).length();
    const Point3 origin = bounds.centroid() + 2.0_f * radius * pt::random_unit_vector(sampler);
    const Point3 target = point_in(sampler, bounds);
    return Ray(origin, target - origin);
}

[[nodiscard]] Ray ray_inside(Sampler& sampler, const Aabb& bounds) {
    const Point3 origin = point_in(sampler, bounds);
    return Ray(origin, pt::random_unit_vector(sampler));
}

// Identity and distance, exactly. What each set writes into the rest of the record is the set's own
// concern, with its own tests; here the question is only whether every child is reachable and the
// nearest one wins.
bool require_same_answer(const Hittable& built, const HittableList& reference, const Ray& r) {
    HitRecord built_rec;
    HitRecord reference_rec;

    const bool built_hit = built.hit(r, visible, built_rec);
    const bool reference_hit = reference.hit(r, visible, reference_rec);

    if (built_hit != reference_hit || (reference_hit && (built_rec.t != reference_rec.t || built_rec.mat != reference_rec.mat))) {
        FAIL("built group and scan disagree for origin (" << r.origin().x() << ", " << r.origin().y() << ", "
                                                          << r.origin().z() << ")  direction (" << r.direction().x()
                                                          << ", " << r.direction().y() << ", " << r.direction().z() << ")");
    }
    return reference_hit;
}

// What a container or a seal built, required to be a tree whose every leaf holds `kind`: the question the
// per-kind tree types used to answer by their type alone.
[[nodiscard]] const pt::Bvh* tree_of_kind(const pt::Hittable* built, PrimitiveKind kind) {
    const auto* tree = dynamic_cast<const pt::Bvh*>(built);
    REQUIRE(tree != nullptr);
    for (const pt::BvhNode& node : tree->topology().nodes) {
        if (node.is_leaf()) REQUIRE(node.kind == kind);
    }
    return tree;
}

} // namespace

TEST_CASE("a mixed group answers exactly like a scan of its children", "[geometry][group_builder]") {
    Children children;
    fill_children(children, 71);

    pt::Arena<Hittable> arena;
    pt::BvhStats stats;

    GroupBuilder builder;
    for (const GeometryRef& ref : children.refs())
        builder.add(ref);
    const Hittable* built = std::move(builder).build(arena, &stats);

    // A sphere set, a quad set, and one tree over the two of them and the instance.
    REQUIRE(tree_of_kind(built, PrimitiveKind::opaque) != nullptr);
    REQUIRE(stats.bvh_count == 3);

    const Aabb bounds = children.list().bounding_box();
    Sampler sampler{pt::sampler_seed(72, 0, 0)};

    int hits = 0;
    for (int i = 0; i < 256; ++i)
        hits += require_same_answer(*built, children.list(), ray_into(sampler, bounds)) ? 1 : 0;
    REQUIRE(hits > 0);

    hits = 0;
    for (int i = 0; i < 256; ++i)
        hits += require_same_answer(*built, children.list(), ray_inside(sampler, bounds)) ? 1 : 0;
    REQUIRE(hits > 0);
}

TEST_CASE("a group of one kind is that kind's set and nothing more", "[geometry][group_builder]") {
    Children children;
    for (int i = 0; i < 20; ++i)
        children.add_sphere(Point3(3.0_f * static_cast<Float>(i), 0, 0), 1.0_f);

    pt::Arena<Hittable> arena;
    pt::BvhStats stats;

    GroupBuilder builder;
    for (const GeometryRef& ref : children.refs())
        builder.add(ref);
    const Hittable* built = std::move(builder).build(arena, &stats);

    // No tree over the set: a world of spheres only is hit through the set itself.
    REQUIRE(tree_of_kind(built, PrimitiveKind::sphere) != nullptr);
    REQUIRE(stats.bvh_count == 1);
}

TEST_CASE("a single opaque child comes back as it is", "[geometry][group_builder]") {
    const Sphere ball{Point3(0, 0, 5), 1.0_f, &shared_material};
    const pt::Instance moved{&ball, Transform::translation(Vec3(1, 0, 0))};

    pt::Arena<Hittable> arena;
    pt::BvhStats stats;

    GroupBuilder builder;
    builder.add(&moved);
    const Hittable* built = std::move(builder).build(arena, &stats);

    // A group around one mesh or one transform is that child: nothing is built, nothing is counted.
    REQUIRE(built == &moved);
    REQUIRE(stats.bvh_count == 0);
}

TEST_CASE("an empty group builds a world that hits nothing", "[geometry][group_builder]") {
    pt::Arena<Hittable> arena;
    pt::BvhStats stats;

    // A scene of media alone stages no object, and its world must still answer every query.
    const Hittable* world = GroupBuilder{}.build(arena, &stats);
    REQUIRE(world != nullptr);
    REQUIRE(world->bounding_box().x.size() < 0.0_f);
    REQUIRE(stats.bvh_count == 0);

    HitRecord rec;
    REQUIRE_FALSE(world->hit(forward, visible, rec));
}

TEST_CASE("seal gives a lone sphere or quad a set of its own", "[geometry][group_builder]") {
    pt::Arena<Hittable> arena;
    pt::BvhStats stats;

    SECTION("a sphere") {
        const Sphere ball{Point3(0, 0, 5), 1.0_f, &shared_material};
        const Hittable* sealed = pt::seal(arena, &ball, &stats);

        const pt::Bvh* set = tree_of_kind(sealed, PrimitiveKind::sphere);
        REQUIRE(set != nullptr);
        REQUIRE(set->topology().nodes.size() == 1);
        REQUIRE(stats.bvh_count == 1);

        HitRecord through_set;
        HitRecord direct;
        REQUIRE(sealed->hit(forward, visible, through_set));
        REQUIRE(ball.hit(forward, visible, direct));
        REQUIRE(through_set.t == direct.t);
        REQUIRE(through_set.mat == direct.mat);
    }

    SECTION("a quad") {
        const Quad square{Point3(-1, -1, 2), Vec3(2, 0, 0), Vec3(0, 2, 0), &shared_material};
        const Hittable* sealed = pt::seal(arena, &square, &stats);

        REQUIRE(tree_of_kind(sealed, PrimitiveKind::quad) != nullptr);
        REQUIRE(stats.bvh_count == 1);

        HitRecord through_set;
        HitRecord direct;
        REQUIRE(sealed->hit(forward, visible, through_set));
        REQUIRE(square.hit(forward, visible, direct));
        REQUIRE(through_set.t == direct.t);
        REQUIRE(through_set.mat == direct.mat);
    }
}

TEST_CASE("seal hands anything else back unchanged", "[geometry][group_builder]") {
    const Sphere ball{Point3(0, 0, 5), 1.0_f, &shared_material};
    const pt::Instance moved{&ball, Transform::translation(Vec3(1, 0, 0))};

    pt::Arena<Hittable> arena;
    pt::BvhStats stats;

    REQUIRE(pt::seal(arena, &moved, &stats) == &moved);

    // Routing follows the type the caller holds: a sphere known only as a Hittable is opaque.
    const Hittable* as_hittable = &ball;
    REQUIRE(pt::seal(arena, as_hittable, &stats) == as_hittable);
    REQUIRE(stats.bvh_count == 0);
}

TEST_CASE("a box in a group joins the group's quad set", "[geometry][group_builder]") {
    pt::Arena<Hittable> arena;
    pt::BvhStats stats;

    const Quad floor{Point3(-5, 0, -5), Vec3(10, 0, 0), Vec3(0, 0, 10), &shared_material};

    GroupBuilder builder;
    builder.add(pt::box(arena, Point3(-1, 0, -1), Point3(1, 2, 1), &shared_material));
    builder.add(&floor);
    const Hittable* built = std::move(builder).build(arena, &stats);

    // Six faces and a floor are seven quads of one kind: one set, and no tree above it.
    const pt::Bvh* set = tree_of_kind(built, PrimitiveKind::quad);
    REQUIRE(set != nullptr);
    REQUIRE(stats.bvh_count == 1);

    std::size_t quads = 0;
    for (const pt::BvhNode& node : set->topology().nodes)
        quads += node.count;
    REQUIRE(quads == 7);

    // The box's top face, from above: reachable through the merged set.
    HitRecord rec;
    REQUIRE(built->hit(Ray(Point3(0, 5, 0), Vec3(0, -1, 0)), visible, rec));
    REQUIRE(rec.t == 3.0_f);
}

TEST_CASE("a box sealed on its own keeps its faces in one leaf", "[geometry][group_builder]") {
    pt::Arena<Hittable> arena;
    pt::BvhStats stats;

    const BoxFaces crate = pt::box(arena, Point3(-1, -1, -1), Point3(1, 1, 1), &shared_material);
    const pt::Bvh* set = tree_of_kind(pt::seal(arena, crate, &stats), PrimitiveKind::quad);

    // What a transform or a medium receives: the box's own set, the six faces tested in a row.
    REQUIRE(set != nullptr);
    REQUIRE(set->topology().nodes.size() == 1);
    REQUIRE(set->topology().nodes.front().count == 6);
    REQUIRE(stats.bvh_count == 1);
}
