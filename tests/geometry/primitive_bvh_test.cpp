#include "pt/core/hit_record.hpp"
#include "pt/core/hittable.hpp"
#include "pt/core/hittable_list.hpp"
#include "pt/geometry/bvh.hpp"
#include "pt/geometry/level.hpp"
#include "pt/geometry/quad.hpp"
#include "pt/geometry/sphere.hpp"
#include "pt/geometry/top_level_bvh.hpp"
#include "pt/materials/lambertian.hpp"
#include "pt/materials/material.hpp"
#include "pt/math/aabb.hpp"
#include "pt/math/color.hpp"
#include "pt/math/constants.hpp"
#include "pt/math/interval.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/sampler.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include "pt/textures/solid_color.hpp"
#include "pt/util/arena.hpp"
#include "support/test_support.hpp"
#include "support/top_level_support.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <cstdint>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace {

using pt::Aabb;
using pt::BvhBuildSettings;
using pt::Float;
using pt::HitRecord;
using pt::HittableList;
using pt::Interval;
using pt::LevelItem;
using pt::Point3;
using pt::Quad;
using pt::Ray;
using pt::Sampler;
using pt::Sphere;
using pt::TopLevelBvh;
using pt::Vec3;
using pt::operator""_f;
using pt_test::top_level_of;

const Interval visible{0.0_f, pt::infinity};

// Rays per family. The defects hunted here - a shape gathered into the wrong slot, a material taken
// from the wrong one, a superseded candidate filled in - show on almost every hit.
constexpr int rays_per_family = 256;

const pt::SolidColor albedo{pt::Color(0.5_f, 0.5_f, 0.5_f)};
const pt::Lambertian shared_material{&albedo};

// Primitives of one kind with a material each, offered two ways: as the items a tree is built from, and as
// the list the reference scans. One material per primitive makes a record name the primitive that answered,
// not merely the distance. Immovable: the materials point at `albedo_`.
template <typename Primitive>
class Population {
public:
    Population() = default;
    Population(const Population&) = delete;
    Population& operator=(const Population&) = delete;

    template <typename... Args>
    void add(Args&&... args) {
        const Primitive* primitive = objects_.create<Primitive>(std::forward<Args>(args)..., new_material());
        items_.emplace_back(primitive);
        list_.add(primitive);
    }

    [[nodiscard]] std::span<const LevelItem> items() const noexcept { return items_; }

    [[nodiscard]] const HittableList& list() const noexcept { return list_; }

    [[nodiscard]] Aabb bounds() const { return list_.bounding_box(); }

private:
    [[nodiscard]] const pt::Material* new_material() { return materials_.create<pt::Lambertian>(&albedo_); }

    const pt::SolidColor albedo_{pt::Color(0.5_f, 0.5_f, 0.5_f)};
    pt::Arena<pt::Material> materials_;
    pt::Arena<pt::Hittable> objects_;
    std::vector<LevelItem> items_;
    HittableList list_;
};

// Braces, not parentheses: the three draws of a braced list are evaluated left to right, so the
// point does not depend on the compiler's choice of argument order.
[[nodiscard]] Point3 point_in(Sampler& sampler, Float lo, Float hi) {
    return Point3{sampler.next_scalar(lo, hi), sampler.next_scalar(lo, hi), sampler.next_scalar(lo, hi)};
}

// Freely overlapping spheres, every third one moving: the sweep bounds and the ray's time both matter.
void fill_spheres(Population<Sphere>& spheres, int count, std::uint64_t seed) {
    Sampler sampler{pt::sampler_seed(seed, 0, 0)};

    for (int i = 0; i < count; ++i) {
        const Point3 center = point_in(sampler, -10.0_f, 10.0_f);
        const Float radius = sampler.next_scalar(0.3_f, 1.5_f);

        if (i % 3 == 0) {
            const Vec3 travel = Vec3::random(-2.0_f, 2.0_f, sampler);
            spheres.add(center, center + travel, radius);
        } else {
            spheres.add(center, radius);
        }
    }
}

// Quads at every orientation, with sheared edges and boxes that overlap. Every fourth one is
// axis-aligned, as a room's walls are: flat on z, so its box carries Aabb's padding.
void fill_quads(Population<Quad>& quads, int count, std::uint64_t seed) {
    Sampler sampler{pt::sampler_seed(seed, 1, 0)};

    for (int i = 0; i < count; ++i) {
        const Point3 corner = point_in(sampler, -10.0_f, 10.0_f);

        if (i % 4 == 0) {
            const Float width = sampler.next_scalar(0.5_f, 4.0_f);
            const Float height = sampler.next_scalar(0.5_f, 4.0_f);
            quads.add(corner, Vec3(width, 0, 0), Vec3(0, height, 0));
        } else {
            const Vec3 u = Vec3::random(-3.0_f, 3.0_f, sampler);
            const Vec3 v = Vec3::random(-3.0_f, 3.0_f, sampler);
            quads.add(corner, u, v);
        }
    }
}

[[nodiscard]] Float bounding_radius(const Aabb& bounds) {
    return 0.5_f * Vec3(bounds.x.size(), bounds.y.size(), bounds.z.size()).length();
}

[[nodiscard]] Point3 point_in(Sampler& sampler, const Aabb& bounds) {
    return Point3{sampler.next_scalar(bounds.x.min, bounds.x.max), sampler.next_scalar(bounds.y.min, bounds.y.max),
                  sampler.next_scalar(bounds.z.min, bounds.z.max)};
}

// Every ray carries its own shutter time, so a moving sphere is met wherever that time puts it.
[[nodiscard]] Ray ray_into(Sampler& sampler, const Aabb& bounds) {
    const Point3 origin = bounds.centroid() + 2.0_f * bounding_radius(bounds) * pt::random_unit_vector(sampler);
    const Point3 target = point_in(sampler, bounds);
    const Float time = sampler.next_scalar();
    return Ray(origin, target - origin, time);
}

[[nodiscard]] Ray ray_inside(Sampler& sampler, const Aabb& bounds) {
    const Point3 origin = point_in(sampler, bounds);
    const Vec3 direction = pt::random_unit_vector(sampler);
    const Float time = sampler.next_scalar();
    return Ray(origin, direction, time);
}

// Values no hit produces: a field the tree forgets to write keeps one and fails the comparison.
[[nodiscard]] HitRecord poisoned_record() {
    static const pt::Lambertian stale{&albedo};
    HitRecord rec;
    rec.p = Point3(-7.0_f, -7.0_f, -7.0_f);
    rec.normal = Vec3(-7.0_f, 0.0_f, 0.0_f);
    rec.geometric_normal = Vec3(0.0_f, -7.0_f, 0.0_f);
    rec.t = -7.0_f;
    rec.u = -7.0_f;
    rec.v = -7.0_f;
    rec.spawn_margin = 0.5_f;
    rec.front_face = false;
    rec.mat = &stale;
    return rec;
}

[[nodiscard]] bool same_vec(const Vec3& a, const Vec3& b) {
    return a.x() == b.x() && a.y() == b.y() && a.z() == b.z();
}

// The first field that differs, or nullptr. Exact throughout: the same free functions on the same
// shape values; only the order candidates are asked in differs, and that decides nothing except at
// exactly equal distances, which random rays do not produce.
[[nodiscard]] const char* first_difference(const HitRecord& actual, const HitRecord& expected) {
    if (actual.t != expected.t) return "t";
    if (actual.u != expected.u) return "u";
    if (actual.v != expected.v) return "v";
    if (actual.front_face != expected.front_face) return "front_face";
    if (actual.mat != expected.mat) return "mat";
    if (actual.spawn_margin != expected.spawn_margin) return "spawn_margin";
    if (!same_vec(actual.p, expected.p)) return "p";
    if (!same_vec(actual.normal, expected.normal)) return "normal";
    if (!same_vec(actual.geometric_normal, expected.geometric_normal)) return "geometric_normal";
    return nullptr;
}

// One ray through both; reports whether the reference hit. The message is built only on failure.
bool require_same_hit(const TopLevelBvh& tree, const HittableList& reference, const Ray& r, const Interval& ray_t) {
    HitRecord tree_rec = poisoned_record();
    HitRecord reference_rec;

    const bool tree_hit = tree.hit(r, ray_t, tree_rec);
    const bool reference_hit = reference.hit(r, ray_t, reference_rec);

    const char* const field =
        tree_hit != reference_hit ? "hit" : (reference_hit ? first_difference(tree_rec, reference_rec) : nullptr);

    if (field != nullptr) {
        FAIL("tree and scan disagree on `" << field << "` for origin (" << r.origin().x() << ", " << r.origin().y()
                                           << ", " << r.origin().z() << ")  direction (" << r.direction().x() << ", "
                                           << r.direction().y() << ", " << r.direction().z() << ")  time " << r.time()
                                           << "  interval [" << ray_t.min << ", " << ray_t.max << "]");
    }
    return reference_hit;
}

template <typename Primitive>
void require_tree_matches_scan(const TopLevelBvh& tree, const Population<Primitive>& population, std::uint64_t seed) {
    const Aabb bounds = population.bounds();
    Sampler sampler{pt::sampler_seed(seed, 0, 0)};

    // Each family has to hit something, or every comparison in it is a miss against a miss.
    int hits = 0;
    for (int i = 0; i < rays_per_family; ++i)
        hits += require_same_hit(tree, population.list(), ray_into(sampler, bounds), visible) ? 1 : 0;
    REQUIRE(hits > 0);

    hits = 0;
    for (int i = 0; i < rays_per_family; ++i)
        hits += require_same_hit(tree, population.list(), ray_inside(sampler, bounds), visible) ? 1 : 0;
    REQUIRE(hits > 0);

    hits = 0;
    for (int i = 0; i < rays_per_family; ++i) {
        // Two statements: argument evaluation order is unspecified, and both draw from the sampler.
        const Ray r = ray_into(sampler, bounds);
        const Interval ray_t(0.001_f, sampler.next_scalar(0.5_f, 10.0_f));
        hits += require_same_hit(tree, population.list(), r, ray_t) ? 1 : 0;
    }
    REQUIRE(hits > 0);
}

} // namespace

TEST_CASE("a tree of spheres answers exactly like a scan of its spheres", "[geometry][primitive_bvh]") {
    const BvhBuildSettings settings =
        GENERATE(BvhBuildSettings{}, BvhBuildSettings{.max_leaf_size = 1}, BvhBuildSettings{.depth_limit = 1});

    Population<Sphere> spheres;
    fill_spheres(spheres, 120, 61);

    const TopLevelBvh tree = top_level_of(spheres.items(), settings);
    require_tree_matches_scan(tree, spheres, 62);
}

TEST_CASE("a tree of quads answers exactly like a scan of its quads", "[geometry][primitive_bvh]") {
    const BvhBuildSettings settings =
        GENERATE(BvhBuildSettings{}, BvhBuildSettings{.max_leaf_size = 1}, BvhBuildSettings{.depth_limit = 1});

    Population<Quad> quads;
    fill_quads(quads, 120, 63);

    const TopLevelBvh tree = top_level_of(quads.items(), settings);
    require_tree_matches_scan(tree, quads, 64);
}

TEST_CASE("a miss leaves the caller's record untouched", "[geometry][primitive_bvh]") {
    // Each ray enters the only leaf's box and misses its primitive, so the leaf test runs and finds
    // no winner. The record is written after traversal, from the winner; without one, nothing is.
    SECTION("a sphere, passed at its box's corner") {
        const Sphere ball{Point3(0, 0, 0), 1.0_f, &shared_material};
        const TopLevelBvh tree = top_level_of({&ball});

        // Inside the box on x and y, outside the sphere: 0.9^2 + 0.9^2 > 1.
        HitRecord rec = poisoned_record();
        REQUIRE_FALSE(tree.hit(Ray(Point3(0.9_f, 0.9_f, -5.0_f), Vec3(0, 0, 1)), visible, rec));
        REQUIRE(rec.t == -7.0_f);
        REQUIRE(rec.spawn_margin == 0.5_f);
    }

    SECTION("a parallelogram, passed in a corner its box covers and it does not") {
        // Points are (2a + b, 2b): at y = 0.2 the quad spans x in [0.1, 2.1], the box [0, 3].
        const Quad sheared{Point3(0, 0, 0), Vec3(2, 0, 0), Vec3(1, 2, 0), &shared_material};
        const TopLevelBvh tree = top_level_of({&sheared});

        HitRecord rec = poisoned_record();
        REQUIRE_FALSE(tree.hit(Ray(Point3(2.8_f, 0.2_f, -5.0_f), Vec3(0, 0, 1)), visible, rec));
        REQUIRE(rec.t == -7.0_f);
        REQUIRE(rec.spawn_margin == 0.5_f);
    }
}

TEST_CASE("an empty span builds an empty tree", "[geometry][primitive_bvh]") {
    const TopLevelBvh tree = top_level_of(std::span<const LevelItem>{});

    REQUIRE(tree.topology().nodes.empty());
    REQUIRE(tree.bounding_box().x.size() < 0.0_f);

    HitRecord rec;
    REQUIRE_FALSE(tree.hit(Ray(Point3(0, 0, 0), Vec3(1, 0, 0)), visible, rec));
}

TEST_CASE("a tree keeps nothing of the spheres it was built from", "[geometry][primitive_bvh]") {
    // Spheres and quads are copied in, with their materials. The sphere is destroyed before the query: a
    // tree that kept a pointer to it would read freed memory here, which the sanitizer build reports and an
    // ordinary build may not.
    std::optional<TopLevelBvh> tree;
    {
        std::vector<Sphere> spheres;
        spheres.reserve(1);
        spheres.emplace_back(Point3(0, 0, 5), 1.0_f, &shared_material);

        tree.emplace(top_level_of({&spheres.front()}));
    }

    HitRecord rec;
    REQUIRE(tree->hit(Ray(Point3(0, 0, 0), Vec3(0, 0, 1)), visible, rec));
    pt_test::require_near(rec.t, 4.0_f);
    REQUIRE(rec.mat == &shared_material);
}
