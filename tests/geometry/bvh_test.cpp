#include "pt/core/hit_record.hpp"
#include "pt/core/hittable.hpp"
#include "pt/core/hittable_list.hpp"
#include "pt/geometry/bvh.hpp"
#include "pt/geometry/mesh.hpp"
#include "pt/geometry/quad.hpp"
#include "pt/geometry/sphere.hpp"
#include "pt/geometry/triangle.hpp"
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
#include "pt/scene/obj_loader.hpp"
#include "pt/textures/solid_color.hpp"
#include "pt/util/arena.hpp"
#include <algorithm>
#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <span>
#include <string>

namespace {

using pt::Aabb;
using pt::Bvh;
using pt::BvhBuildSettings;
using pt::Float;
using pt::HitRecord;
using pt::Hittable;
using pt::HittableList;
using pt::Interval;
using pt::Point3;
using pt::Ray;
using pt::Sampler;
using pt::Vec3;
using pt::operator""_f;

// The interval the renderer itself uses: the lower bound is the shadow-acne epsilon.
const Interval visible{0.001_f, pt::infinity};

// Owns every primitive and material one test scene needs. Arena is the engine's own
// ownership model, so addresses stay valid as the scene grows - a vector of
// primitives would invalidate them on reallocation.
//
// Neither copyable nor movable: the materials point at `albedo_`, which is a member,
// so a move would leave them pointing into the moved-from object.
class TestScene {
public:
    TestScene() = default;
    TestScene(const TestScene&) = delete;
    TestScene& operator=(const TestScene&) = delete;

    void add_sphere(const Point3& center, Float radius) {
        list_.add(objects_.create<pt::Sphere>(center, radius, new_material()));
    }

    void add_quad(const Point3& q, const Vec3& u, const Vec3& v) {
        list_.add(objects_.create<pt::Quad>(q, u, v, new_material()));
    }

    void add_triangle(const Point3& a, const Point3& b, const Point3& c) {
        list_.add(objects_.create<pt::Triangle>(a, b, c, new_material()));
    }

    [[nodiscard]] const HittableList& list() const noexcept { return list_; }

    [[nodiscard]] std::span<const Hittable* const> objects() const noexcept { return list_.objects(); }

    [[nodiscard]] Aabb bounds() const { return list_.bounding_box(); }

private:
    // One material per primitive: a hit record then identifies which primitive answered,
    // not merely how far away the answer was. Never dereferenced - it is an identity tag.
    [[nodiscard]] const pt::Material* new_material() { return materials_.create<pt::Lambertian>(&albedo_); }

    const pt::SolidColor albedo_{pt::Color(0.5_f, 0.5_f, 0.5_f)};
    pt::Arena<pt::Material> materials_;
    pt::Arena<Hittable> objects_;
    HittableList list_;
};

// Scenes are filled in place rather than returned: TestScene is immovable, so a factory
// returning a named local could not compile.
//
// Positions come from a continuous distribution, so no two surfaces coincide. That is a
// requirement, not an accident: at exactly equal t the winner depends on visiting order,
// and the two paths visit in different orders (see the file's test for that boundary).
void fill_sphere_cloud(TestScene& scene, int count, std::uint64_t seed) {
    Sampler sampler{pt::sampler_seed(seed, 0, 0)};

    for (int i = 0; i < count; ++i) {
        const Point3 center(sampler.next_scalar(-10.0_f, 10.0_f), sampler.next_scalar(-10.0_f, 10.0_f),
                            sampler.next_scalar(-10.0_f, 10.0_f));
        scene.add_sphere(center, sampler.next_scalar(0.3_f, 1.2_f));
    }
}

// Mixed primitive types: their bounding boxes behave differently (a planar quad has a
// padded degenerate axis), which produces a differently shaped tree from the same count.
void fill_mixed_scene(TestScene& scene, int count, std::uint64_t seed) {
    Sampler sampler{pt::sampler_seed(seed, 1, 0)};

    for (int i = 0; i < count; ++i) {
        const Point3 anchor(sampler.next_scalar(-10.0_f, 10.0_f), sampler.next_scalar(-10.0_f, 10.0_f),
                            sampler.next_scalar(-10.0_f, 10.0_f));

        switch (i % 3) {
        case 0:
            scene.add_sphere(anchor, sampler.next_scalar(0.3_f, 1.2_f));
            break;
        case 1:
            scene.add_quad(anchor, Vec3::random(-3.0_f, 3.0_f, sampler), Vec3::random(-3.0_f, 3.0_f, sampler));
            break;
        default:
            scene.add_triangle(anchor, anchor + Vec3::random(-3.0_f, 3.0_f, sampler),
                               anchor + Vec3::random(-3.0_f, 3.0_f, sampler));
            break;
        }
    }
}

// A 3x3x3 lattice of spheres on integer coordinates, hand-built so the exact coordinates
// are known. The degenerate-ray test needs to place a ray origin exactly on a bounding
// box face, which random geometry cannot express.
void fill_lattice(TestScene& scene) {
    for (int i = -1; i <= 1; ++i) {
        for (int j = -1; j <= 1; ++j) {
            for (int k = -1; k <= 1; ++k) {
                scene.add_sphere(Point3(static_cast<Float>(i), static_cast<Float>(j), static_cast<Float>(k)), 0.4_f);
            }
        }
    }
}

// Spheres on a coarse grid with jitter, sized so that no two can overlap: spacing 4 against
// a maximum radius of 1.2 leaves at least 0.6 of clearance. Non-overlap is what makes the
// reachability test below valid - a ray leaving a sphere's own centre can only meet that
// sphere first if nothing penetrates it.
void fill_separated_spheres(TestScene& scene, int per_axis, std::uint64_t seed) {
    Sampler sampler{pt::sampler_seed(seed, 2, 0)};
    constexpr Float spacing = 4.0_f;

    for (int i = 0; i < per_axis; ++i) {
        for (int j = 0; j < per_axis; ++j) {
            for (int k = 0; k < per_axis; ++k) {
                const Point3 cell(spacing * static_cast<Float>(i), spacing * static_cast<Float>(j),
                                  spacing * static_cast<Float>(k));
                const Vec3 jitter(sampler.next_scalar(-0.5_f, 0.5_f), sampler.next_scalar(-0.5_f, 0.5_f),
                                  sampler.next_scalar(-0.5_f, 0.5_f));

                scene.add_sphere(cell + jitter, sampler.next_scalar(0.3_f, 1.2_f));
            }
        }
    }
}

// Mesh geometry is a class of its own here: neighbouring triangles share edges and
// vertices, so leaf bounds genuinely interpenetrate. The randomly placed triangles above
// never touch each other, which leaves that case untested.
//
// Meshes are held by the scene rather than the arena: Mesh is neither copyable nor
// movable and MeshTriangle handles are bound to its address, so it must be built in place
// and outlive the handles. A deque never relocates its elements, unlike a vector.
class MeshScene {
public:
    MeshScene() = default;
    MeshScene(const MeshScene&) = delete;
    MeshScene& operator=(const MeshScene&) = delete;

    void add_obj(const std::string& filename) {
        const std::filesystem::path path = std::filesystem::path(PT_ASSETS_DIR) / filename;

        meshes_.emplace_back(pt::load_obj(path), materials_.create<pt::Lambertian>(&albedo_));

        for (const Hittable* triangle : pt::mesh_triangles(objects_, meshes_.back())->objects()) {
            list_.add(triangle);
        }
    }

    [[nodiscard]] const HittableList& list() const noexcept { return list_; }

    [[nodiscard]] std::span<const Hittable* const> objects() const noexcept { return list_.objects(); }

    [[nodiscard]] Aabb bounds() const { return list_.bounding_box(); }

private:
    const pt::SolidColor albedo_{pt::Color(0.5_f, 0.5_f, 0.5_f)};
    pt::Arena<pt::Material> materials_;
    pt::Arena<Hittable> objects_;
    std::deque<pt::Mesh> meshes_;
    HittableList list_;
};

[[nodiscard]] Float bounding_radius(const Aabb& bounds) {
    return 0.5_f * Vec3(bounds.x.size(), bounds.y.size(), bounds.z.size()).length();
}

// Origin on a shell around the scene, aimed at a random interior point: most of these hit,
// so this family carries the bulk of the comparison.
[[nodiscard]] Ray ray_into_scene(Sampler& sampler, const Aabb& bounds) {
    const Point3 origin = bounds.centroid() + 2.0_f * bounding_radius(bounds) * pt::random_unit_vector(sampler);
    const Point3 target(sampler.next_scalar(bounds.x.min, bounds.x.max), sampler.next_scalar(bounds.y.min, bounds.y.max),
                        sampler.next_scalar(bounds.z.min, bounds.z.max));

    return Ray(origin, target - origin); // Deliberately not normalised: the engine's rays are not either.
}

// Origin inside the scene: exercises the ray_t.min side of the slab test, and leaves part
// of the geometry behind the ray where neither path may report it.
[[nodiscard]] Ray ray_inside_scene(Sampler& sampler, const Aabb& bounds) {
    const Point3 origin(sampler.next_scalar(bounds.x.min, bounds.x.max), sampler.next_scalar(bounds.y.min, bounds.y.max),
                        sampler.next_scalar(bounds.z.min, bounds.z.max));

    return Ray(origin, pt::random_unit_vector(sampler));
}

// Aimed outwards from outside: the root box rejects it before the loop starts.
[[nodiscard]] Ray ray_away_from_scene(Sampler& sampler, const Aabb& bounds) {
    const Vec3 outward = pt::random_unit_vector(sampler);
    return Ray(bounds.centroid() + 2.0_f * bounding_radius(bounds) * outward, outward);
}

void require_same_vec(const Vec3& actual, const Vec3& expected) {
    REQUIRE(actual.x() == expected.x());
    REQUIRE(actual.y() == expected.y());
    REQUIRE(actual.z() == expected.z());
}

// Runs one query through both paths and requires them to agree exactly.
void require_same_hit(const Bvh& bvh, const HittableList& reference, const Ray& r, const Interval& ray_t) {
    HitRecord bvh_rec;
    HitRecord reference_rec;

    const bool bvh_hit = bvh.hit(r, ray_t, bvh_rec);
    const bool reference_hit = reference.hit(r, ray_t, reference_rec);

    INFO("origin (" << r.origin().x() << ", " << r.origin().y() << ", " << r.origin().z() << ")  direction ("
                    << r.direction().x() << ", " << r.direction().y() << ", " << r.direction().z() << ")");

    REQUIRE(bvh_hit == reference_hit);
    if (!reference_hit) return;

    // Identity first: the same primitive answered, not just one at a similar distance.
    REQUIRE(bvh_rec.mat == reference_rec.mat);

    // Exact comparison, not a tolerance. Both paths run the same intersection code on the
    // same ray, so an accepted hit carries bit-identical values; the interval only decides
    // acceptance, it never enters the arithmetic. A tolerance here would mask exactly the
    // bug this test exists to catch - traversal returning a nearby but different surface.
    REQUIRE(bvh_rec.t == reference_rec.t);
    REQUIRE(bvh_rec.u == reference_rec.u);
    REQUIRE(bvh_rec.v == reference_rec.v);
    REQUIRE(bvh_rec.front_face == reference_rec.front_face);
    require_same_vec(bvh_rec.p, reference_rec.p);
    require_same_vec(bvh_rec.normal, reference_rec.normal);
}

constexpr int ray_count = 2000;

} // namespace

TEST_CASE("a bvh answers exactly like a brute-force list", "[geometry][bvh]") {
    // The build settings change the tree's shape but must not change a single answer.
    // The extremes matter more than the default: leaf size 1 produces the deepest tree and
    // the heaviest traversal, an oversized leaf size collapses the tree into a linear scan,
    // and the inverted cost ratio makes splitting look expensive so leaves form early.
    const BvhBuildSettings settings =
        GENERATE(BvhBuildSettings{},                                                     // production defaults
                 BvhBuildSettings{.max_leaf_size = 1},                                   // one primitive per leaf
                 BvhBuildSettings{.bin_count = 2},                                       // lower clamp
                 BvhBuildSettings{.bin_count = 32},                                      // upper clamp
                 BvhBuildSettings{.max_leaf_size = 1 << 20},                             // clamped: cost alone decides
                 BvhBuildSettings{.traversal_cost = 8.0_f, .intersection_cost = 1.0_f}); // inverted cost

    TestScene scene;
    fill_sphere_cloud(scene, 200, 20260823);

    const Bvh bvh(scene.objects(), settings);
    Sampler sampler{pt::sampler_seed(7, 0, 0)};

    SECTION("rays aimed through the scene") {
        for (int i = 0; i < ray_count; ++i) {
            require_same_hit(bvh, scene.list(), ray_into_scene(sampler, scene.bounds()), visible);
        }
    }

    SECTION("rays starting inside the scene") {
        for (int i = 0; i < ray_count; ++i) {
            require_same_hit(bvh, scene.list(), ray_inside_scene(sampler, scene.bounds()), visible);
        }
    }

    SECTION("rays aimed away from the scene") {
        for (int i = 0; i < ray_count; ++i) {
            require_same_hit(bvh, scene.list(), ray_away_from_scene(sampler, scene.bounds()), visible);
        }
    }

    SECTION("rays whose interval ends inside the scene") {
        // A finite ray_t.max is what traversal starts `closest` at, so this is the only
        // family that exercises the deferred far child being discarded without a visit.
        for (int i = 0; i < ray_count; ++i) {
            const Ray r = ray_into_scene(sampler, scene.bounds());
            require_same_hit(bvh, scene.list(), r, Interval(0.001_f, sampler.next_scalar(1.0_f, 30.0_f)));
        }
    }
}

TEST_CASE("a bvh over mixed primitive types answers exactly like a brute-force list", "[geometry][bvh]") {
    const BvhBuildSettings settings = GENERATE(BvhBuildSettings{}, BvhBuildSettings{.max_leaf_size = 1});

    TestScene scene;
    fill_mixed_scene(scene, 150, 424242);

    const Bvh bvh(scene.objects(), settings);
    Sampler sampler{pt::sampler_seed(11, 0, 0)};

    for (int i = 0; i < ray_count; ++i) {
        require_same_hit(bvh, scene.list(), ray_into_scene(sampler, scene.bounds()), visible);
    }
}

TEST_CASE("degenerate ray directions traverse the tree exactly like the list", "[geometry][bvh]") {
    TestScene scene;
    fill_lattice(scene);

    const Bvh bvh(scene.objects());

    // Lattice sphere boxes span [c - 0.4, c + 0.4] on every axis. An axis-aligned ray has an
    // infinite reciprocal on the two flat axes; when its origin also sits exactly on a slab
    // face, the slab test evaluates 0 * inf = NaN. The comparison chain in Aabb::intersect
    // exists for that case - it keeps the box conservatively accepted, so the primitive test
    // decides. A branchless fmin/fmax rewrite would reject the box and lose the hit, which is what
    // makes these rays worth pinning down ahead of any SIMD or branchless rewrite of that loop.
    const Ray r = GENERATE(Ray(Point3(-10.0_f, 0.0_f, 0.0_f), Vec3(1.0_f, 0.0_f, 0.0_f)),      // through the centres
                           Ray(Point3(-10.0_f, 0.4_f, 0.0_f), Vec3(1.0_f, 0.0_f, 0.0_f)),      // NaN on y, grazes a sphere
                           Ray(Point3(-10.0_f, -0.4_f, 0.4_f), Vec3(1.0_f, 0.0_f, 0.0_f)),     // NaN on y and z, hits nothing
                           Ray(Point3(0.0_f, -10.0_f, 0.0_f), Vec3(0.0_f, 1.0_f, 0.0_f)),      // along +y
                           Ray(Point3(0.0_f, 0.0_f, 10.0_f), Vec3(0.0_f, 0.0_f, -1.0_f)),      // along -z
                           Ray(Point3(0.0_f, 0.0_f, 0.0_f), Vec3(1.0_f, 0.0_f, 0.0_f)),        // origin inside a sphere
                           Ray(Point3(-10.0_f, -10.0_f, -10.0_f), Vec3(1.0_f, 1.0_f, 1.0_f))); // no infinite reciprocal

    require_same_hit(bvh, scene.list(), r, visible);
}

TEST_CASE("an empty bvh is a valid empty tree", "[geometry][bvh]") {
    // The scene format allows an empty group, so this reaches the renderer rather than
    // being a purely theoretical input.
    const Bvh bvh{std::span<const Hittable* const>{}};

    REQUIRE(bvh.node_count() == 0);
    REQUIRE(bvh.leaf_count() == 0);
    REQUIRE(bvh.max_depth() == 0);

    // A default Aabb has min above max on every axis, so no point is inside it.
    REQUIRE(bvh.bounding_box().x.size() < 0.0_f);

    HitRecord rec;
    REQUIRE_FALSE(bvh.hit(Ray(Point3(0.0_f, 0.0_f, 0.0_f), Vec3(1.0_f, 0.0_f, 0.0_f)), visible, rec));
}

TEST_CASE("a single-primitive bvh is one leaf and still finds it", "[geometry][bvh]") {
    TestScene scene;
    scene.add_sphere(Point3(0.0_f, 0.0_f, 0.0_f), 1.0_f);

    const Bvh bvh(scene.objects());

    // The pointer tree this replaced linked a lone primitive into both children and tested
    // it twice on every visit - docs/benchmark.md records a single-sphere scene at 1.5
    // leaf tests per ray. A leaf is a range now: one primitive, one node, one leaf, one test.
    REQUIRE(bvh.node_count() == 1);
    REQUIRE(bvh.leaf_count() == 1);
    REQUIRE(bvh.max_depth() == 0);

    Sampler sampler{pt::sampler_seed(3, 0, 0)};
    for (int i = 0; i < 200; ++i) {
        require_same_hit(bvh, scene.list(), ray_into_scene(sampler, scene.bounds()), visible);
    }
}

TEST_CASE("the tree's shape obeys its own invariants", "[geometry][bvh]") {
    constexpr int primitive_count = 200;

    TestScene scene;
    fill_sphere_cloud(scene, primitive_count, 777);

    SECTION("the root box is the union the list computes, exactly") {
        const Bvh bvh(scene.objects());

        const Aabb tree = bvh.bounding_box();
        const Aabb list = scene.bounds();

        // Both fold the same primitives in the same order through the same Aabb(a, b)
        // constructor: the build takes the root's bounds before it permutes anything.
        // An approximate match here would mean one of those three claims is false.
        REQUIRE(tree.x.min == list.x.min);
        REQUIRE(tree.x.max == list.x.max);
        REQUIRE(tree.y.min == list.y.min);
        REQUIRE(tree.y.max == list.y.max);
        REQUIRE(tree.z.min == list.z.min);
        REQUIRE(tree.z.max == list.z.max);
    }

    SECTION("every interior node has exactly two children") {
        const BvhBuildSettings settings = GENERATE(BvhBuildSettings{}, BvhBuildSettings{.max_leaf_size = 1},
                                                   BvhBuildSettings{.bin_count = 2}, BvhBuildSettings{.bin_count = 32});

        const Bvh bvh(scene.objects(), settings);

        // A binary tree whose interior nodes all have two children holds 2L - 1 nodes for
        // L leaves. A dangling single-child branch or a stray empty node breaks the identity,
        // and neither is visible from a hit query.
        REQUIRE(bvh.leaf_count() > 0);
        REQUIRE(bvh.node_count() == 2 * bvh.leaf_count() - 1);
    }

    SECTION("a leaf size of one puts every primitive in its own leaf") {
        const Bvh bvh(scene.objects(), BvhBuildSettings{.max_leaf_size = 1});

        // Leaves partition the primitives, so with at most one each the counts must match
        // exactly. This is the numeric proof that the build's permutation neither drops a
        // primitive nor stores one twice.
        REQUIRE(bvh.leaf_count() == static_cast<std::size_t>(primitive_count));
        REQUIRE(bvh.node_count() == static_cast<std::size_t>(2 * primitive_count - 1));
    }

    SECTION("a traversal cost that dwarfs intersection collapses the tree into one leaf") {
        // Both gates in leaf_is_cheaper have to open: the size cap is checked first, the
        // cost second. This is the degenerate end of the parameter space, where the BVH
        // becomes a linear scan and only the leaf path of traversal runs.
        const Bvh bvh(scene.objects(), BvhBuildSettings{.traversal_cost = 1.0e9_f, .max_leaf_size = 1 << 20});

        REQUIRE(bvh.node_count() == 1);
        REQUIRE(bvh.leaf_count() == 1);
        REQUIRE(bvh.max_depth() == 0);
    }

    SECTION("the tree stays within the traversal stack's fixed depth") {
        // 64 duplicates max_traversal_depth, which is private to bvh.cpp. The constructor
        // asserts the same bound, but assertions are compiled out of Release, where an
        // over-deep tree would overflow a fixed-size stack silently. Deepest leaf sizes
        // first: a cap of one produces the tallest tree this builder can make.
        const Bvh bvh(scene.objects(), BvhBuildSettings{.max_leaf_size = 1});

        REQUIRE(bvh.max_depth() <= 64);
    }
}

TEST_CASE("every primitive in the scene is reachable through the tree", "[geometry][bvh]") {
    TestScene scene;
    fill_separated_spheres(scene, 4, 31337);

    const Bvh bvh(scene.objects());
    Sampler direction_sampler{pt::sampler_seed(17, 0, 0)};

    // Ray queries sample the tree; they do not enumerate it. A primitive dropped by the
    // build is only caught here, by asking for each one by name: a ray leaving a sphere's
    // own centre must come back with that sphere, and the spheres do not overlap.
    for (const Hittable* primitive : scene.objects()) {
        const Ray r(primitive->bounding_box().centroid(), pt::random_unit_vector(direction_sampler));

        HitRecord direct;
        REQUIRE(primitive->hit(r, visible, direct));

        HitRecord through_tree;
        REQUIRE(bvh.hit(r, visible, through_tree));

        REQUIRE(through_tree.mat == direct.mat);
        REQUIRE(through_tree.t == direct.t);
    }
}

TEST_CASE("nested bvhs answer exactly like a flat brute-force list", "[geometry][bvh]") {
    TestScene scene;
    fill_sphere_cloud(scene, 90, 5150);

    const std::span<const Hittable* const> primitives = scene.objects();
    const std::size_t group_size = primitives.size() / 3;

    // A Bvh is itself a Hittable, and the loader relies on that: meshes and groups become
    // subtrees inside the scene's own tree. Grouping changes the traversal completely -
    // three inner trees behind one outer one - and must change nothing else.
    const Bvh left(primitives.subspan(0, group_size));
    const Bvh middle(primitives.subspan(group_size, group_size));
    const Bvh right(primitives.subspan(2 * group_size));

    HittableList groups;
    groups.add(&left);
    groups.add(&middle);
    groups.add(&right);

    const Bvh nested(groups.objects());

    Sampler sampler{pt::sampler_seed(13, 0, 0)};
    for (int i = 0; i < ray_count; ++i) {
        // The reference stays the flat list of all 90 spheres: it knows nothing of groups.
        require_same_hit(nested, scene.list(), ray_into_scene(sampler, scene.bounds()), visible);
    }
}

TEST_CASE("make_bvh folds each tree's counts into the shared stats", "[geometry][bvh]") {
    TestScene scene;
    fill_sphere_cloud(scene, 60, 909);

    pt::Arena<Hittable> arena;
    pt::BvhStats stats;

    const Bvh* first = pt::make_bvh(arena, scene.list(), &stats);

    REQUIRE(stats.bvh_count == 1);
    REQUIRE(stats.node_count == first->node_count());
    REQUIRE(stats.leaf_count == first->leaf_count());
    REQUIRE(stats.max_depth == first->max_depth());

    const auto after_first = stats.build_time;

    // A scene builds one tree per mesh and group, so these are running totals rather than
    // assignments - and they are what the baseline report publishes. Depth is a maximum,
    // not a sum: two shallow trees do not add up to a deep one.
    const Bvh* second = pt::make_bvh(arena, scene.list(), &stats);

    REQUIRE(stats.bvh_count == 2);
    REQUIRE(stats.node_count == first->node_count() + second->node_count());
    REQUIRE(stats.leaf_count == first->leaf_count() + second->leaf_count());
    REQUIRE(stats.max_depth == std::max(first->max_depth(), second->max_depth()));
    REQUIRE(stats.build_time >= after_first);

    // The default argument is the path a caller that wants no statistics takes.
    REQUIRE(pt::make_bvh(arena, scene.list()) != nullptr);
}

TEST_CASE("a bvh over loaded mesh geometry answers exactly like a brute-force list", "[geometry][bvh][mesh]") {
    const std::string filename = GENERATE("unit_cube.obj", "tetrahedron.obj");

    // Leaf size one puts every triangle in its own leaf, so sibling leaves overlap wherever
    // triangles share an edge; the default groups them and hides that overlap inside a leaf.
    const BvhBuildSettings settings = GENERATE(BvhBuildSettings{}, BvhBuildSettings{.max_leaf_size = 1});

    MeshScene scene;
    scene.add_obj(filename);

    const Bvh bvh(scene.objects(), settings);
    Sampler sampler{pt::sampler_seed(23, 0, 0)};

    // Rays are aimed at interior points, never at edges. A ray hitting a shared edge meets
    // two triangles at exactly the same distance, where the winner is decided by visiting
    // order and the two paths legitimately disagree - a documented limit of this comparison,
    // not a defect to assert against.
    for (int i = 0; i < ray_count; ++i) {
        require_same_hit(bvh, scene.list(), ray_into_scene(sampler, scene.bounds()), visible);
    }

    SECTION("and from inside the mesh, where rays leave through its own faces") {
        for (int i = 0; i < ray_count; ++i) {
            require_same_hit(bvh, scene.list(), ray_inside_scene(sampler, scene.bounds()), visible);
        }
    }
}
