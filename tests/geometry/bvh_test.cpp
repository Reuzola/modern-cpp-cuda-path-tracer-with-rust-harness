#include "pt/core/hit_record.hpp"
#include "pt/core/hittable.hpp"
#include "pt/core/hittable_list.hpp"
#include "pt/geometry/bvh.hpp"
#include "pt/geometry/mesh.hpp"
#include "pt/geometry/primitive_kind.hpp"
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
#include <array>
#include <bit>
#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <numeric>
#include <optional>
#include <span>
#include <string>
#include <vector>

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

// The interval the renderer uses: intersections are queried from zero, and the spawn origin carries the offset.
const Interval visible{0.0_f, pt::infinity};

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

// Mesh geometry is a class of its own here: neighbouring triangles share edges and vertices, so leaf
// bounds genuinely interpenetrate. The randomly placed triangles above never touch each other, which
// leaves that case untested. Each mesh triangle becomes a standalone Triangle: the generic tree is what
// is under test, and a mesh's own tree has its tests in triangle_bvh_test.
class MeshScene {
public:
    MeshScene() = default;
    MeshScene(const MeshScene&) = delete;
    MeshScene& operator=(const MeshScene&) = delete;

    void add_obj(const std::string& filename) {
        const std::filesystem::path path = std::filesystem::path(PT_ASSETS_DIR) / filename;

        // Read only to resolve the indices: Triangle copies its corners, so the mesh may go.
        const pt::Mesh mesh(pt::load_obj(path), nullptr);
        const pt::Material* mat = materials_.create<pt::Lambertian>(&albedo_);

        for (std::size_t i = 0; i < mesh.triangle_count(); ++i) {
            const auto [a, b, c] = mesh.triangle(i);
            list_.add(objects_.create<pt::Triangle>(a, b, c, mat));
        }
    }

    [[nodiscard]] const HittableList& list() const noexcept { return list_; }

    [[nodiscard]] std::span<const Hittable* const> objects() const noexcept { return list_.objects(); }

    [[nodiscard]] Aabb bounds() const { return list_.bounding_box(); }

private:
    const pt::SolidColor albedo_{pt::Color(0.5_f, 0.5_f, 0.5_f)};
    pt::Arena<pt::Material> materials_;
    pt::Arena<Hittable> objects_;
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

    // The spawn data too. A primitive that skipped writing its margin would leak the
    // previous candidate's, and the two paths visit candidates in different orders,
    // so they would leak different ones.
    REQUIRE(bvh_rec.spawn_margin == reference_rec.spawn_margin);
    require_same_vec(bvh_rec.geometric_normal, reference_rec.geometric_normal);
}

// Random boxes, built directly: the builder sees nothing but bounds, so its test needs nothing else.
// Sizes start at zero, so some boxes are flat on an axis and carry Aabb's padding, as planar primitives do.
[[nodiscard]] std::vector<Aabb> random_boxes(int count, std::uint64_t seed) {
    Sampler sampler{pt::sampler_seed(seed, 3, 0)};

    std::vector<Aabb> boxes;
    boxes.reserve(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
        const Point3 corner(sampler.next_scalar(-10.0_f, 10.0_f), sampler.next_scalar(-10.0_f, 10.0_f),
                            sampler.next_scalar(-10.0_f, 10.0_f));
        boxes.emplace_back(corner, corner + Vec3::random(0.0_f, 2.0_f, sampler));
    }
    return boxes;
}

// The label the bounds-only tests build under. Which single kind it is changes nothing: a build over one
// kind takes every decision it took before kinds existed.
constexpr pt::PrimitiveKind any_kind = pt::PrimitiveKind::sphere;

// Kinds drawn independently of the boxes, so neighbours in space differ as often as not: the builder
// meets mixed ranges at every depth, not only near the root.
[[nodiscard]] std::vector<pt::PrimitiveKind> random_kinds(std::size_t count, std::uint64_t seed) {
    constexpr std::array all{pt::PrimitiveKind::sphere, pt::PrimitiveKind::quad, pt::PrimitiveKind::triangle,
                             pt::PrimitiveKind::opaque};
    Sampler sampler{pt::sampler_seed(seed, 4, 0)};

    std::vector<pt::PrimitiveKind> kinds;
    kinds.reserve(count);
    for (std::size_t i = 0; i < count; ++i)
        kinds.push_back(all[sampler.next_below(static_cast<std::uint32_t>(all.size()))]);
    return kinds;
}

// Every leaf holds primitives of one kind, and it is the kind the leaf records. Leaves are found by
// scanning the node array: this needs no structure, only leaves. FAIL rather than a REQUIRE per slot,
// which would cost more than the build it checks.
void require_single_kind_leaves(const pt::BvhBuildResult& build, std::span<const pt::PrimitiveKind> kinds) {
    for (const pt::BvhNode& node : build.topology.nodes) {
        if (!node.is_leaf()) continue;
        for (std::size_t slot = node.offset; slot < node.offset + node.count; ++slot) {
            if (kinds[build.primitive_order[slot]] != node.kind) FAIL("the leaf at slot " << node.offset << " mixes kinds");
        }
    }
}

[[nodiscard]] bool same_node(const pt::BvhNode& a, const pt::BvhNode& b) {
    return a.offset == b.offset && a.count == b.count && a.kind == b.kind && a.bbox.x.min == b.bbox.x.min &&
           a.bbox.x.max == b.bbox.x.max && a.bbox.y.min == b.bbox.y.min && a.bbox.y.max == b.bbox.y.max &&
           a.bbox.z.min == b.bbox.z.min && a.bbox.z.max == b.bbox.z.max;
}

[[nodiscard]] Vec3 reciprocal(const Vec3& d) {
    return Vec3(1.0_f / d.x(), 1.0_f / d.y(), 1.0_f / d.z());
}

// Boxes as primitives in their own right: the nearest entry distance inside the interval. The slab test
// narrows nothing itself, so the interval is narrowed here, the way a primitive's own test would be.
[[nodiscard]] std::optional<Float> nearest_box(std::span<const Aabb> boxes, const Ray& r, const Vec3& inv_dir, Interval ray_t) {
    std::optional<Float> nearest;
    for (const Aabb& box : boxes) {
        if (const std::optional<Float> t = box.intersect(r.origin(), inv_dir, ray_t)) {
            nearest = t;
            ray_t.max = *t;
        }
    }
    return nearest;
}

[[nodiscard]] Aabb union_of(std::span<const Aabb> boxes) {
    Aabb all;
    for (const Aabb& box : boxes)
        all = Aabb(all, box);
    return all;
}

// The leaf-test contract is checked when the template is named, not discovered as a wrong answer later.
static_assert(pt::BvhLeafTest<decltype([](const pt::BvhNode&, const Interval&) { return std::optional<Float>{}; })>);
static_assert(!pt::BvhLeafTest<decltype([](const pt::BvhNode&, const Interval&) { return true; })>);
static_assert(!pt::BvhLeafTest<decltype([](const pt::BvhNode&) { return std::optional<Float>{}; })>);

void require_same_box(const Aabb& actual, const Aabb& expected) {
    REQUIRE(actual.x.min == expected.x.min);
    REQUIRE(actual.x.max == expected.x.max);
    REQUIRE(actual.y.min == expected.y.min);
    REQUIRE(actual.y.max == expected.y.max);
    REQUIRE(actual.z.min == expected.z.min);
    REQUIRE(actual.z.max == expected.z.max);
}

// The run of leaf slots a subtree covers.
struct SlotRange {
    std::size_t first{};
    std::size_t count{};
};

struct WalkTotals {
    std::size_t leaves{};
    int max_depth{};
};

// Walks the subtree at `index` and returns the slots it covers, checking on the way that a left
// subtree's slots are followed directly by the right one's, and that every node's box is exactly the
// union of its primitives' bounds. Union is min and max, which round nothing, so exact is right.
SlotRange check_subtree(const pt::BvhBuildResult& build, std::span<const Aabb> bounds, std::uint32_t index, int depth, WalkTotals& totals) {
    const pt::BvhNode& node = build.topology.nodes[index];
    totals.max_depth = std::max(totals.max_depth, depth);

    SlotRange range;
    if (node.is_leaf()) {
        ++totals.leaves;
        range = SlotRange{.first = node.offset, .count = node.count};
    } else {
        const SlotRange left = check_subtree(build, bounds, index + 1, depth + 1, totals);
        const SlotRange right = check_subtree(build, bounds, node.offset, depth + 1, totals);
        REQUIRE(left.first + left.count == right.first);
        range = SlotRange{.first = left.first, .count = left.count + right.count};
    }

    REQUIRE(range.first + range.count <= build.primitive_order.size());

    Aabb expected;
    for (std::size_t slot = range.first; slot < range.first + range.count; ++slot) {
        expected = Aabb(expected, bounds[build.primitive_order[slot]]);
    }
    require_same_box(node.bbox, expected);

    return range;
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
        // The builder caps every tree at bvh_max_depth, the stack's capacity. The constructor asserts the
        // same bound, but assertions are compiled out of Release, so it is checked here as well. Deepest
        // leaf size first: a cap of one produces the tallest tree this builder can make.
        const Bvh bvh(scene.objects(), BvhBuildSettings{.max_leaf_size = 1});

        REQUIRE(bvh.max_depth() <= pt::bvh_max_depth);
    }
}

TEST_CASE("a depth limit caps the tree without changing a single answer", "[geometry][bvh]") {
    // 500 rather than the 200 of the other cases: a cloud this size builds three levels past log2 n,
    // where 200 builds only one, which leaves a limit room to sit strictly between the two.
    constexpr int primitive_count = 500;

    TestScene scene;
    fill_sphere_cloud(scene, primitive_count, 4141);

    Sampler sampler{pt::sampler_seed(29, 0, 0)};

    SECTION("a limit the SAH would exceed is met by halving below it") {
        // Above log2(500) = 9, so the root still splits by SAH and only the deep end of the tree is
        // halved: the mixed case, where a budget miscounted by one level would show.
        constexpr int limit = 10;

        // Not vacuous: unconstrained, this cloud builds deeper than the limit.
        const Bvh unlimited(scene.objects(), BvhBuildSettings{.max_leaf_size = 1});
        REQUIRE(unlimited.max_depth() > limit);

        const Bvh capped(scene.objects(), BvhBuildSettings{.max_leaf_size = 1, .depth_limit = limit});

        REQUIRE(capped.max_depth() <= limit);

        // Halving reorders nothing and keeps the tree binary: every sphere still in its own leaf.
        REQUIRE(capped.leaf_count() == static_cast<std::size_t>(primitive_count));
        REQUIRE(capped.node_count() == 2 * capped.leaf_count() - 1);

        for (int i = 0; i < ray_count; ++i) {
            require_same_hit(capped, scene.list(), ray_into_scene(sampler, scene.bounds()), visible);
        }
    }

    SECTION("a limit no tree can meet halves from the root") {
        // Every node is over budget, so no SAH split is ever taken. Halving 500 primitives reaches ranges
        // of three and four after seven levels, and those fit the default leaf size of four: 128 leaves,
        // all at depth 7. The exact shape is what proves the SAH was bypassed at every node.
        const Bvh halved(scene.objects(), BvhBuildSettings{.depth_limit = 1});

        REQUIRE(halved.max_depth() == 7);
        REQUIRE(halved.leaf_count() == 128);
        REQUIRE(halved.node_count() == 255);

        for (int i = 0; i < ray_count; ++i) {
            require_same_hit(halved, scene.list(), ray_into_scene(sampler, scene.bounds()), visible);
        }
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

TEST_CASE("a topology built from bounds and kinds holds every primitive exactly once", "[geometry][bvh]") {
    // The forced-halving path is included: it builds leaves the SAH never chose, and the same layout
    // contract has to hold for them. So is a cost that makes every range a leaf, where only kinds split.
    const BvhBuildSettings settings =
        GENERATE(BvhBuildSettings{}, BvhBuildSettings{.max_leaf_size = 1}, BvhBuildSettings{.bin_count = 2},
                 BvhBuildSettings{.depth_limit = 1}, BvhBuildSettings{.traversal_cost = 1.0e9_f, .max_leaf_size = 1 << 20});

    // One kind is the path every tree took before kinds existed; random kinds put mixed ranges at every depth.
    const bool mixed = GENERATE(false, true);

    const std::vector<Aabb> boxes = random_boxes(500, 8086);
    const std::vector<pt::PrimitiveKind> kinds = mixed ? random_kinds(boxes.size(), 8087) : std::vector(boxes.size(), any_kind);
    const pt::BvhBuildResult build = pt::build_bvh_topology(boxes, kinds, settings);

    SECTION("the primitive order is a permutation of the input") {
        std::vector<std::uint32_t> sorted = build.primitive_order;
        std::ranges::sort(sorted);

        std::vector<std::uint32_t> expected(boxes.size());
        std::iota(expected.begin(), expected.end(), 0U);

        REQUIRE(sorted == expected);
    }

    SECTION("subtrees cover contiguous runs, and the root covers them all") {
        WalkTotals totals;
        const SlotRange root = check_subtree(build, boxes, 0, 0, totals);

        REQUIRE(root.first == 0);
        REQUIRE(root.count == boxes.size());

        // The reported shape is the shape the walk found, not a separately kept tally.
        REQUIRE(totals.leaves == build.topology.leaf_count);
        REQUIRE(totals.max_depth == build.topology.max_depth);
        REQUIRE(build.topology.nodes.size() == 2 * build.topology.leaf_count - 1);

        // The bound BvhBuildSettings promises. Splits along kinds spend depth like any SAH split, and
        // a mixed range out of budget is halved like any other, so they must keep it too.
        REQUIRE(totals.max_depth <= std::max(settings.depth_limit, std::bit_width(boxes.size() - 1)));
    }

    SECTION("no leaf mixes kinds, and each records the one it holds") {
        require_single_kind_leaves(build, kinds);
    }
}

TEST_CASE("a leaf the cost would choose is split along kinds instead", "[geometry][bvh]") {
    using enum pt::PrimitiveKind;

    // Three kinds interleaved, first met in an order that is not the enumerators' own: the tree must take
    // its order from the input, as every other tie in the build does.
    constexpr std::array cycle{quad, opaque, sphere};
    const std::vector<Aabb> boxes = random_boxes(30, 5577);
    std::vector<pt::PrimitiveKind> kinds;
    for (std::size_t i = 0; i < boxes.size(); ++i)
        kinds.push_back(cycle[i % cycle.size()]);

    // A traversal cost that dwarfs intersection makes every range cheaper as a leaf, so the only splits
    // left are the ones a leaf of one kind forces: the root separates the first kind from the other two,
    // its right child separates those. Three leaves, one per kind, two levels deep.
    const pt::BvhBuildResult build =
        pt::build_bvh_topology(boxes, kinds, BvhBuildSettings{.traversal_cost = 1.0e9_f, .max_leaf_size = 1 << 20});
    const std::vector<pt::BvhNode>& nodes = build.topology.nodes;

    REQUIRE(nodes.size() == 5);
    REQUIRE(build.topology.leaf_count == 3);
    REQUIRE(build.topology.max_depth == 2);

    // Depth first: root, its left leaf, then the right subtree's interior node and its two leaves.
    REQUIRE((nodes[1].is_leaf() && nodes[1].kind == quad && nodes[1].count == 10));
    REQUIRE(!nodes[2].is_leaf());
    REQUIRE((nodes[3].is_leaf() && nodes[3].kind == opaque && nodes[3].count == 10));
    REQUIRE((nodes[4].is_leaf() && nodes[4].kind == sphere && nodes[4].count == 10));

    require_single_kind_leaves(build, kinds);
}

TEST_CASE("a build over one kind labels every leaf with it", "[geometry][bvh]") {
    const pt::PrimitiveKind kind =
        GENERATE(pt::PrimitiveKind::sphere, pt::PrimitiveKind::quad, pt::PrimitiveKind::triangle, pt::PrimitiveKind::opaque);
    const std::vector<Aabb> boxes = random_boxes(200, 9001);

    const pt::BvhBuildResult build = pt::build_bvh_topology(boxes, kind);
    for (const pt::BvhNode& node : build.topology.nodes) {
        if (node.is_leaf()) REQUIRE(node.kind == kind);
    }

    // The single-kind overload is the general one with the same label everywhere. Kept as a check, not
    // left to the implementation: a later shortcut that skips the kinds must still build this tree.
    const std::vector<pt::PrimitiveKind> labels(boxes.size(), kind);
    const pt::BvhBuildResult general = pt::build_bvh_topology(boxes, labels);

    REQUIRE(build.primitive_order == general.primitive_order);
    REQUIRE(build.topology.nodes.size() == general.topology.nodes.size());
    for (std::size_t i = 0; i < general.topology.nodes.size(); ++i) {
        if (!same_node(build.topology.nodes[i], general.topology.nodes[i])) FAIL("trees first differ at node " << i);
    }
}

TEST_CASE("an empty input builds an empty topology", "[geometry][bvh]") {
    const pt::BvhBuildResult build = pt::build_bvh_topology(std::span<const Aabb>{}, any_kind);

    REQUIRE(build.topology.nodes.empty());
    REQUIRE(build.topology.leaf_count == 0);
    REQUIRE(build.topology.max_depth == 0);
    REQUIRE(build.primitive_order.empty());
}

TEST_CASE("traversal over a bare topology finds what a brute-force scan finds", "[geometry][bvh]") {
    const BvhBuildSettings settings =
        GENERATE(BvhBuildSettings{}, BvhBuildSettings{.max_leaf_size = 1}, BvhBuildSettings{.depth_limit = 1});

    const std::vector<Aabb> boxes = random_boxes(300, 2718);
    const pt::BvhBuildResult build = pt::build_bvh_topology(boxes, any_kind, settings);

    // Laid out in the build's order, as any owner of a topology lays out its primitives.
    std::vector<Aabb> ordered;
    ordered.reserve(boxes.size());
    for (const std::uint32_t index : build.primitive_order)
        ordered.push_back(boxes[index]);

    const Aabb bounds = union_of(boxes);
    Sampler sampler{pt::sampler_seed(31, 0, 0)};

    const auto require_same_nearest = [&](const Ray& r, const Interval& ray_t) {
        const Vec3 inv_dir = reciprocal(r.direction());

        // Only reports that narrowed are kept: traversal accepts a leaf's answer only when it is
        // nearer than everything before it, so the last one kept is the nearest overall.
        std::optional<Float> nearest_seen;
        const bool hit = pt::bvh_closest_hit(build.topology, r, ray_t, [&](const pt::BvhNode& leaf, const Interval& open) {
            const std::optional<Float> t = nearest_box(std::span(ordered).subspan(leaf.offset, leaf.count), r, inv_dir, open);
            if (t) nearest_seen = t;
            return t;
        });

        const std::optional<Float> scanned = nearest_box(boxes, r, inv_dir, ray_t);

        // Exact: both paths run the same slab test on the same boxes and the same ray. The tree only
        // decides which boxes are asked, never what they answer. Ties need no care: equal distances
        // are the same value whichever box reported them.
        REQUIRE(hit == scanned.has_value());
        REQUIRE(nearest_seen == scanned);
    };

    SECTION("rays aimed through the boxes") {
        for (int i = 0; i < ray_count; ++i)
            require_same_nearest(ray_into_scene(sampler, bounds), visible);
    }

    SECTION("rays starting among the boxes") {
        for (int i = 0; i < ray_count; ++i)
            require_same_nearest(ray_inside_scene(sampler, bounds), visible);
    }

    SECTION("rays whose interval ends among the boxes") {
        for (int i = 0; i < ray_count; ++i) {
            require_same_nearest(ray_into_scene(sampler, bounds), Interval(0.001_f, sampler.next_scalar(1.0_f, 30.0_f)));
        }
    }
}

TEST_CASE("the interval handed to leaves only ever narrows", "[geometry][bvh]") {
    const std::vector<Aabb> boxes = random_boxes(300, 1618);
    const pt::BvhBuildResult build = pt::build_bvh_topology(boxes, any_kind, BvhBuildSettings{.max_leaf_size = 1});

    std::vector<Aabb> ordered;
    ordered.reserve(boxes.size());
    for (const std::uint32_t index : build.primitive_order)
        ordered.push_back(boxes[index]);

    Sampler sampler{pt::sampler_seed(37, 0, 0)};
    const Interval ray_t{0.0_f, 50.0_f};

    for (int i = 0; i < 200; ++i) {
        const Ray r = ray_into_scene(sampler, union_of(boxes));
        const Vec3 inv_dir = reciprocal(r.direction());

        std::vector<Interval> seen;
        static_cast<void>(pt::bvh_closest_hit(build.topology, r, ray_t, [&](const pt::BvhNode& leaf, const Interval& open) {
            seen.push_back(open);
            return nearest_box(std::span(ordered).subspan(leaf.offset, leaf.count), r, inv_dir, open);
        }));

        if (seen.empty()) continue;

        // The first leaf sees the caller's interval untouched, and the lower bound never moves.
        REQUIRE(seen.front().max == ray_t.max);
        for (std::size_t k = 0; k < seen.size(); ++k) {
            REQUIRE(seen[k].min == ray_t.min);
            if (k > 0) REQUIRE(seen[k].max <= seen[k - 1].max);
        }
    }
}

TEST_CASE("an empty topology asks no leaf", "[geometry][bvh]") {
    int calls = 0;
    const bool hit = pt::bvh_closest_hit(pt::BvhTopology{}, Ray(Point3(0, 0, 0), Vec3(1, 0, 0)), visible,
                                         [&](const pt::BvhNode&, const Interval&) -> std::optional<Float> {
                                             ++calls;
                                             return 0.0_f;
                                         });

    REQUIRE_FALSE(hit);
    REQUIRE(calls == 0);
}
