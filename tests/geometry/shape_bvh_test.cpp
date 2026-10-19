#include "pt/core/hit_record.hpp"
#include "pt/core/hittable.hpp"
#include "pt/core/hittable_list.hpp"
#include "pt/geometry/bvh.hpp"
#include "pt/geometry/quad.hpp"
#include "pt/geometry/shape_bvh.hpp"
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
#include "pt/math/vec3.hpp"
#include "pt/textures/solid_color.hpp"
#include "pt/util/arena.hpp"
#include "support/test_support.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace {

using pt::Aabb;
using pt::BvhBuildSettings;
using pt::BvhTopology;
using pt::Float;
using pt::HitRecord;
using pt::HittableList;
using pt::Interval;
using pt::Point3;
using pt::Quad;
using pt::QuadBvh;
using pt::Ray;
using pt::Sampler;
using pt::Sphere;
using pt::SphereBvh;
using pt::Vec3;
using pt::operator""_f;

const Interval visible{0.0_f, pt::infinity};

// Rays per family. The defects hunted here - a shape gathered into the wrong slot, a material taken
// from the wrong one, a superseded candidate filled in - show on almost every hit.
constexpr int rays_per_family = 256;

const pt::SolidColor albedo{pt::Color(0.5_f, 0.5_f, 0.5_f)};
const pt::Lambertian shared_material{&albedo};

// Primitives of one kind with a material each, offered both ways: as the typed span a set is built
// from, and as the list the reference scans. One material per primitive makes a record name the
// primitive that answered, not merely the distance. Immovable: the materials point at `albedo_`.
template <typename Primitive>
class Population {
public:
    Population() = default;
    Population(const Population&) = delete;
    Population& operator=(const Population&) = delete;

    template <typename... Args>
    void add(Args&&... args) {
        const Primitive* primitive = objects_.create<Primitive>(std::forward<Args>(args)..., new_material());
        primitives_.push_back(primitive);
        list_.add(primitive);
    }

    [[nodiscard]] std::span<const Primitive* const> primitives() const noexcept { return primitives_; }

    [[nodiscard]] const HittableList& list() const noexcept { return list_; }

    [[nodiscard]] Aabb bounds() const { return list_.bounding_box(); }

private:
    [[nodiscard]] const pt::Material* new_material() { return materials_.create<pt::Lambertian>(&albedo_); }

    const pt::SolidColor albedo_{pt::Color(0.5_f, 0.5_f, 0.5_f)};
    pt::Arena<pt::Material> materials_;
    pt::Arena<pt::Hittable> objects_;
    std::vector<const Primitive*> primitives_;
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

// Values no hit produces: a field the set forgets to write keeps one and fails the comparison.
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
bool require_same_hit(const pt::Hittable& set, const HittableList& reference, const Ray& r, const Interval& ray_t) {
    HitRecord set_rec = poisoned_record();
    HitRecord reference_rec;

    const bool set_hit = set.hit(r, ray_t, set_rec);
    const bool reference_hit = reference.hit(r, ray_t, reference_rec);

    const char* const field =
        set_hit != reference_hit ? "hit" : (reference_hit ? first_difference(set_rec, reference_rec) : nullptr);

    if (field != nullptr) {
        FAIL("set and scan disagree on `" << field << "` for origin (" << r.origin().x() << ", " << r.origin().y()
                                          << ", " << r.origin().z() << ")  direction (" << r.direction().x() << ", "
                                          << r.direction().y() << ", " << r.direction().z() << ")  time " << r.time()
                                          << "  interval [" << ray_t.min << ", " << ray_t.max << "]");
    }
    return reference_hit;
}

template <typename Primitive>
void require_set_matches_scan(const pt::ShapeBvh<Primitive>& set, const Population<Primitive>& population, std::uint64_t seed) {
    const Aabb bounds = population.bounds();
    Sampler sampler{pt::sampler_seed(seed, 0, 0)};

    // Each family has to hit something, or every comparison in it is a miss against a miss.
    int hits = 0;
    for (int i = 0; i < rays_per_family; ++i)
        hits += require_same_hit(set, population.list(), ray_into(sampler, bounds), visible) ? 1 : 0;
    REQUIRE(hits > 0);

    hits = 0;
    for (int i = 0; i < rays_per_family; ++i)
        hits += require_same_hit(set, population.list(), ray_inside(sampler, bounds), visible) ? 1 : 0;
    REQUIRE(hits > 0);

    hits = 0;
    for (int i = 0; i < rays_per_family; ++i) {
        // Two statements: argument evaluation order is unspecified, and both draw from the sampler.
        const Ray r = ray_into(sampler, bounds);
        const Interval ray_t(0.001_f, sampler.next_scalar(0.5_f, 10.0_f));
        hits += require_same_hit(set, population.list(), r, ray_t) ? 1 : 0;
    }
    REQUIRE(hits > 0);
}

[[nodiscard]] bool same_node(const pt::BvhNode& a, const pt::BvhNode& b) {
    return a.offset == b.offset && a.count == b.count && a.bbox.x.min == b.bbox.x.min && a.bbox.x.max == b.bbox.x.max &&
           a.bbox.y.min == b.bbox.y.min && a.bbox.y.max == b.bbox.y.max && a.bbox.z.min == b.bbox.z.min &&
           a.bbox.z.max == b.bbox.z.max;
}

void require_same_topology(const BvhTopology& actual, const BvhTopology& expected) {
    REQUIRE(actual.nodes.size() == expected.nodes.size());
    REQUIRE(actual.leaf_count == expected.leaf_count);
    REQUIRE(actual.max_depth == expected.max_depth);

    for (std::size_t i = 0; i < expected.nodes.size(); ++i) {
        if (!same_node(actual.nodes[i], expected.nodes[i])) FAIL("trees first differ at node " << i);
    }
}

} // namespace

TEST_CASE("a sphere set answers exactly like a scan of its spheres", "[geometry][shape_bvh]") {
    const BvhBuildSettings settings =
        GENERATE(BvhBuildSettings{}, BvhBuildSettings{.max_leaf_size = 1}, BvhBuildSettings{.depth_limit = 1});

    Population<Sphere> spheres;
    fill_spheres(spheres, 120, 61);

    const SphereBvh set(spheres.primitives(), settings);
    require_set_matches_scan(set, spheres, 62);
}

TEST_CASE("a quad set answers exactly like a scan of its quads", "[geometry][shape_bvh]") {
    const BvhBuildSettings settings =
        GENERATE(BvhBuildSettings{}, BvhBuildSettings{.max_leaf_size = 1}, BvhBuildSettings{.depth_limit = 1});

    Population<Quad> quads;
    fill_quads(quads, 120, 63);

    const QuadBvh set(quads.primitives(), settings);
    require_set_matches_scan(set, quads, 64);
}

TEST_CASE("a set builds the tree a generic bvh builds over the same objects", "[geometry][shape_bvh]") {
    // The scene relies on this: a world of spheres only that moves from a generic tree to a set keeps
    // its shape, its node tests and its leaf tests, so only the cost of a test changes.
    SECTION("spheres") {
        Population<Sphere> spheres;
        fill_spheres(spheres, 90, 65);

        const SphereBvh set(spheres.primitives());
        const pt::Bvh generic(spheres.list().objects());
        require_same_topology(set.topology(), generic.topology());
    }

    SECTION("quads") {
        Population<Quad> quads;
        fill_quads(quads, 90, 66);

        const QuadBvh set(quads.primitives());
        const pt::Bvh generic(quads.list().objects());
        require_same_topology(set.topology(), generic.topology());
    }
}

TEST_CASE("a miss leaves the caller's record untouched", "[geometry][shape_bvh]") {
    // Each ray enters the only leaf's box and misses its primitive, so the leaf test runs and finds
    // no winner. The record is written after traversal, from the winner; without one, nothing is.
    SECTION("a sphere, passed at its box's corner") {
        const Sphere ball{Point3(0, 0, 0), 1.0_f, &shared_material};
        const std::vector<const Sphere*> one{&ball};
        const SphereBvh set(one);

        // Inside the box on x and y, outside the sphere: 0.9^2 + 0.9^2 > 1.
        HitRecord rec = poisoned_record();
        REQUIRE_FALSE(set.hit(Ray(Point3(0.9_f, 0.9_f, -5.0_f), Vec3(0, 0, 1)), visible, rec));
        REQUIRE(rec.t == -7.0_f);
        REQUIRE(rec.spawn_margin == 0.5_f);
    }

    SECTION("a parallelogram, passed in a corner its box covers and it does not") {
        // Points are (2a + b, 2b): at y = 0.2 the quad spans x in [0.1, 2.1], the box [0, 3].
        const Quad sheared{Point3(0, 0, 0), Vec3(2, 0, 0), Vec3(1, 2, 0), &shared_material};
        const std::vector<const Quad*> one{&sheared};
        const QuadBvh set(one);

        HitRecord rec = poisoned_record();
        REQUIRE_FALSE(set.hit(Ray(Point3(2.8_f, 0.2_f, -5.0_f), Vec3(0, 0, 1)), visible, rec));
        REQUIRE(rec.t == -7.0_f);
        REQUIRE(rec.spawn_margin == 0.5_f);
    }
}

TEST_CASE("an empty span builds an empty set", "[geometry][shape_bvh]") {
    const SphereBvh set(std::span<const Sphere* const>{});

    REQUIRE(set.topology().nodes.empty());
    REQUIRE(set.bounding_box().x.size() < 0.0_f);

    HitRecord rec;
    REQUIRE_FALSE(set.hit(Ray(Point3(0, 0, 0), Vec3(1, 0, 0)), visible, rec));
}

TEST_CASE("a set keeps nothing of the objects it was built from", "[geometry][shape_bvh]") {
    // The objects are destroyed before the query. A set that kept a pointer to them would read freed
    // memory here, which the sanitizer build reports and an ordinary build may not.
    std::optional<SphereBvh> set;
    {
        std::vector<Sphere> spheres;
        spheres.reserve(1);
        spheres.emplace_back(Point3(0, 0, 5), 1.0_f, &shared_material);

        const std::vector<const Sphere*> pointers{&spheres.front()};
        set.emplace(pointers);
    }

    HitRecord rec;
    REQUIRE(set->hit(Ray(Point3(0, 0, 0), Vec3(0, 0, 1)), visible, rec));
    pt_test::require_near(rec.t, 4.0_f);
    REQUIRE(rec.mat == &shared_material);
}

TEST_CASE("make_shape_bvh folds its tree into the shared stats", "[geometry][shape_bvh]") {
    Population<Quad> quads;
    fill_quads(quads, 40, 67);

    pt::Arena<pt::Hittable> arena;
    pt::BvhStats stats;

    const QuadBvh* first = pt::make_shape_bvh<Quad>(arena, quads.primitives(), &stats);
    REQUIRE(stats.bvh_count == 1);
    REQUIRE(stats.node_count == first->topology().nodes.size());
    REQUIRE(stats.leaf_count == first->topology().leaf_count);
    REQUIRE(stats.max_depth == first->topology().max_depth);

    // Totals across trees, as a scene with several sets reports them.
    const QuadBvh* second = pt::make_shape_bvh<Quad>(arena, quads.primitives(), &stats, BvhBuildSettings{.max_leaf_size = 1});
    REQUIRE(stats.bvh_count == 2);
    REQUIRE(stats.node_count == first->topology().nodes.size() + second->topology().nodes.size());
    REQUIRE(stats.leaf_count == first->topology().leaf_count + second->topology().leaf_count);

    REQUIRE(pt::make_shape_bvh<Quad>(arena, quads.primitives()) != nullptr);
}
