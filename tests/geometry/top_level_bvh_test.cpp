#include "pt/core/hit_record.hpp"
#include "pt/core/hittable.hpp"
#include "pt/geometry/bottom_level_bvh.hpp"
#include "pt/geometry/bvh.hpp"
#include "pt/geometry/bvh_node.hpp"
#include "pt/geometry/instance.hpp"
#include "pt/geometry/level.hpp"
#include "pt/geometry/mesh.hpp"
#include "pt/geometry/primitive_kind.hpp"
#include "pt/geometry/quad.hpp"
#include "pt/geometry/sphere.hpp"
#include "pt/geometry/top_level_bvh.hpp"
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
#include "pt/math/transform.hpp"
#include "pt/math/vec3.hpp"
#include "pt/textures/solid_color.hpp"
#include "pt/util/arena.hpp"
#include "support/placed_reference.hpp"
#include "support/test_support.hpp"
#include <array>
#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace {

using pt::Aabb;
using pt::BottomLevelBvh;
using pt::BvhBuildSettings;
using pt::HitRecord;
using pt::Hittable;
using pt::Instance;
using pt::Interval;
using pt::LevelItem;
using pt::Mesh;
using pt::Point3;
using pt::Ray;
using pt::Sampler;
using pt::TopLevelBvh;
using pt::Transform;
using pt::Vec3;
using pt::operator""_f;
using pt_test::Placed;
using pt_test::random_triangles;
using pt_test::ray_inside;
using pt_test::ray_into;
using pt_test::reference_hit;
using pt_test::ReferenceHit;
using pt_test::same_bits;
using pt_test::same_record;

const Interval visible{0.0_f, pt::infinity};

// A scene of both levels and every kind: world-space spheres, quads and two meshes beside five instances of two
// bottom-level trees, one mixing spheres with a mesh and one of quads only. Every object gets a material of
// its own, so a record names the object it came from. Immovable: the trees point at the objects.
class TwoLevelScene {
public:
    explicit TwoLevelScene(std::uint64_t seed) {
        Sampler sampler{pt::sampler_seed(seed, 0, 0)};

        std::vector<LevelItem> spheres_and_mesh;
        spheres_and_mesh.reserve(12 + 1);
        for (int i = 0; i < 12; ++i)
            spheres_and_mesh.emplace_back(add_sphere(sampler, bottom_contents_[0]));
        spheres_and_mesh.emplace_back(add_mesh(sampler, bottom_contents_[0]));

        std::vector<LevelItem> quads;
        quads.reserve(10);
        for (int i = 0; i < 10; ++i)
            quads.emplace_back(add_quad(sampler, bottom_contents_[1]));

        // A rotation with a non-uniform scale, a plain translation, a mirror, a tilt and a turn: every kind of
        // matrix the loader can compose, so a wrong space for the fill shows up as a wrong point or normal.
        const std::array placements{
            Transform::translation(Vec3(14, 0, 0)) * Transform::rotation_y(35.0_f) * Transform::scaling(Vec3(1.5_f, 0.7_f, 1.2_f)),
            Transform::translation(Vec3(-14, 3, 2)),
            Transform::translation(Vec3(0, 14, -3)) * Transform::scaling(Vec3(-1, 1, 1)),
            Transform::translation(Vec3(2, -14, 4)) * Transform::rotation_x(-60.0_f) * Transform::rotation_z(20.0_f),
            Transform::translation(Vec3(-3, -2, 14)) * Transform::rotation_z(75.0_f),
        };
        const std::array<std::uint32_t, 5> names{0, 1, 1, 0, 0};

        // Interleaved: world-space primitives before, between and after the instances, the way a scene
        // graph's order mixes them.
        for (int i = 0; i < 20; ++i)
            top_items_.emplace_back(add_sphere(sampler, top_placed_));
        top_items_.emplace_back(Instance(placements[0], names[0]));
        for (int i = 0; i < 20; ++i)
            top_items_.emplace_back(add_quad(sampler, top_placed_));
        top_items_.emplace_back(add_mesh(sampler, top_placed_));
        for (std::size_t k = 1; k < 5; ++k)
            top_items_.emplace_back(Instance(placements[k], names[k]));

        // A second mesh in the same level: its triangles interleave with the first one's in the leaves, so
        // only a per-slot record of the mesh gets their materials right.
        top_items_.emplace_back(add_mesh(sampler, top_placed_));

        bottom_items_[0] = std::move(spheres_and_mesh);
        bottom_items_[1] = std::move(quads);

        // The reference sees every object of a tree once per instance of it, through that instance.
        reference_ = top_placed_;
        for (std::size_t k = 0; k < 5; ++k) {
            for (const Placed& object : bottom_contents_[names[k]])
                reference_.push_back(Placed{.object = object.object, .instance = Instance(placements[k], names[k])});
        }
    }

    TwoLevelScene(const TwoLevelScene&) = delete;
    TwoLevelScene& operator=(const TwoLevelScene&) = delete;

    [[nodiscard]] TopLevelBvh build(const BvhBuildSettings& top_settings, const BvhBuildSettings& bottom_settings) const {
        std::vector<BottomLevelBvh> bottom;
        bottom.emplace_back(bottom_items_[0], bottom_settings);
        bottom.emplace_back(bottom_items_[1], bottom_settings);
        return TopLevelBvh(top_items_, std::move(bottom), top_settings);
    }

    [[nodiscard]] std::span<const Placed> reference() const noexcept { return reference_; }

    // Whether a reference index is one of the world-space objects rather than one seen through an instance.
    [[nodiscard]] bool is_top_level(std::size_t index) const noexcept { return index < top_placed_.size(); }

private:
    [[nodiscard]] const pt::Material* new_material() { return materials_.create<pt::Lambertian>(&albedo_); }

    const pt::Sphere* add_sphere(Sampler& sampler, std::vector<Placed>& placed) {
        const Point3 centre{sampler.next_scalar(-10.0_f, 10.0_f), sampler.next_scalar(-10.0_f, 10.0_f), sampler.next_scalar(-10.0_f, 10.0_f)};
        const pt::Sphere* sphere = objects_.create<pt::Sphere>(centre, sampler.next_scalar(0.3_f, 1.5_f), new_material());
        placed.push_back(Placed{.object = sphere, .instance = std::nullopt});
        return sphere;
    }

    const pt::Quad* add_quad(Sampler& sampler, std::vector<Placed>& placed) {
        const Point3 corner{sampler.next_scalar(-10.0_f, 10.0_f), sampler.next_scalar(-10.0_f, 10.0_f), sampler.next_scalar(-10.0_f, 10.0_f)};
        const Vec3 u = Vec3::random(-3.0_f, 3.0_f, sampler);
        const Vec3 v = Vec3::random(-3.0_f, 3.0_f, sampler);
        const pt::Quad* quad = objects_.create<pt::Quad>(corner, u, v, new_material());
        placed.push_back(Placed{.object = quad, .instance = std::nullopt});
        return quad;
    }

    // The mesh as the trees see it, and each of its triangles as a standalone Triangle for the reference.
    const Mesh* add_mesh(Sampler& sampler, std::vector<Placed>& placed) {
        const pt::Material* material = new_material();
        meshes_.push_back(std::make_unique<Mesh>(random_triangles(sampler, 24), material));
        const Mesh* mesh = meshes_.back().get();

        for (std::size_t i = 0; i < mesh->triangle_count(); ++i) {
            const auto [a, b, c] = mesh->triangle(i);
            placed.push_back(Placed{.object = objects_.create<pt::Triangle>(a, b, c, material), .instance = std::nullopt});
        }
        return mesh;
    }

    const pt::SolidColor albedo_{pt::Color(0.5_f, 0.5_f, 0.5_f)};
    pt::Arena<pt::Material> materials_;
    pt::Arena<Hittable> objects_;
    std::vector<std::unique_ptr<Mesh>> meshes_;

    std::vector<LevelItem> top_items_;
    std::array<std::vector<LevelItem>, 2> bottom_items_;
    std::vector<Placed> top_placed_;
    std::array<std::vector<Placed>, 2> bottom_contents_;
    std::vector<Placed> reference_;
};

// What a stream of rays found, by where the winner sat: a test that never sees both kinds of winner
// has not exercised both fills.
struct Winners {
    int top_level{};
    int instanced{};
};

void require_same_answer(const TopLevelBvh& tree, const TwoLevelScene& scene, const Ray& r, const Interval& ray_t, Winners& winners) {
    HitRecord rec;
    const bool hit = tree.hit(r, ray_t, rec);
    const std::optional<ReferenceHit> expected = reference_hit(scene.reference(), r, ray_t);

    INFO("origin (" << r.origin().x() << ", " << r.origin().y() << ", " << r.origin().z() << ")  direction ("
                    << r.direction().x() << ", " << r.direction().y() << ", " << r.direction().z() << ")");
    REQUIRE(hit == expected.has_value());
    if (!expected) return;

    REQUIRE(same_record(rec, expected->rec));
    ++(scene.is_top_level(expected->index) ? winners.top_level : winners.instanced);
}

} // namespace

TEST_CASE("a two-level tree answers exactly like a scan of every placed primitive", "[geometry][top_level_bvh]") {
    // Leaves of one, the default, and one leaf per kind wherever the cost allows: the last puts several
    // instances in one leaf, where each must be asked only for what is nearer than the one before it.
    const BvhBuildSettings top_settings = GENERATE(BvhBuildSettings{}, BvhBuildSettings{.max_leaf_size = 1},
                                                   BvhBuildSettings{.traversal_cost = 1.0e9_f, .max_leaf_size = 64});
    const TwoLevelScene scene(41);
    const TopLevelBvh tree = scene.build(top_settings, BvhBuildSettings{});

    REQUIRE(tree.instance_count() == 5);
    REQUIRE(tree.bottom_levels().size() == 2);

    const Aabb bounds = tree.bounding_box();
    Sampler sampler{pt::sampler_seed(42, 0, 0)};
    Winners winners;

    SECTION("rays aimed into the scene") {
        for (int i = 0; i < 512; ++i)
            require_same_answer(tree, scene, ray_into(sampler, bounds), visible, winners);
    }

    SECTION("rays starting inside it") {
        for (int i = 0; i < 512; ++i)
            require_same_answer(tree, scene, ray_inside(sampler, bounds), visible, winners);
    }

    SECTION("rays whose interval ends inside it") {
        for (int i = 0; i < 512; ++i)
            require_same_answer(tree, scene, ray_into(sampler, bounds), Interval(0.001_f, sampler.next_scalar(5.0_f, 60.0_f)), winners);
    }

    REQUIRE(winners.top_level > 0);
    REQUIRE(winners.instanced > 0);
}

TEST_CASE("the top level's box holds every placed primitive", "[geometry][top_level_bvh]") {
    const TwoLevelScene scene(43);
    const TopLevelBvh tree = scene.build(BvhBuildSettings{}, BvhBuildSettings{});
    const Aabb box = tree.bounding_box();

    // Containment, not equality: an instance is bounded by its tree's box carried through the transform,
    // which a rotation loosens beyond the union of the primitives' own boxes.
    for (const Placed& placed : scene.reference()) {
        const Aabb own = placed.instance ? placed.instance->transform().apply_bounds(placed.object->bounding_box())
                                         : placed.object->bounding_box();
        REQUIRE(box.x.min <= own.x.min);
        REQUIRE(box.y.min <= own.y.min);
        REQUIRE(box.z.min <= own.z.min);
        REQUIRE(box.x.max >= own.x.max);
        REQUIRE(box.y.max >= own.y.max);
        REQUIRE(box.z.max >= own.z.max);
    }
}

TEST_CASE("a miss through either level leaves the record untouched", "[geometry][top_level_bvh]") {
    const TwoLevelScene scene(44);
    const TopLevelBvh tree = scene.build(BvhBuildSettings{}, BvhBuildSettings{});

    // Far outside everything, pointing away from it.
    HitRecord rec;
    rec.t = 42.0_f;
    REQUIRE_FALSE(tree.hit(Ray(Point3(0, 0, 500), Vec3(0, 0, 1)), visible, rec));
    REQUIRE(rec.t == 42.0_f);
}

TEST_CASE("an empty top level misses everything", "[geometry][top_level_bvh]") {
    const TopLevelBvh empty;

    HitRecord rec;
    REQUIRE_FALSE(empty.hit(Ray(Point3(0, 0, 0), Vec3(0, 0, 1)), visible, rec));
    REQUIRE(empty.bounding_box().x.size() < 0.0_f);
    REQUIRE(empty.instance_count() == 0);
}

TEST_CASE("a level of one mesh builds exactly the tree its triangles build", "[geometry][top_level_bvh]") {
    Sampler sampler{pt::sampler_seed(45, 0, 0)};
    const pt::SolidColor albedo{pt::Color(0.5_f, 0.5_f, 0.5_f)};
    const pt::Lambertian material{&albedo};
    const Mesh mesh(random_triangles(sampler, 200), &material);

    std::vector<Aabb> bounds;
    for (std::size_t i = 0; i < mesh.triangle_count(); ++i) {
        const auto [a, b, c] = mesh.triangle(i);
        bounds.push_back(pt::triangle_bounds(a, b, c));
    }
    const pt::BvhBuildResult direct = pt::build_bvh_topology(bounds, pt::PrimitiveKind::triangle);

    const std::array<LevelItem, 1> items{&mesh};
    const BottomLevelBvh level(items);

    // A mesh reaches a level as one item and is expanded there. The expansion must hand the build the same
    // bounds in the same order a build over the mesh alone sees, or a scene whose only content is a mesh
    // would get a different tree - and different counters - for nothing.
    const std::vector<pt::BvhNode>& actual = level.topology().nodes;
    REQUIRE(actual.size() == direct.topology.nodes.size());
    for (std::size_t i = 0; i < actual.size(); ++i) {
        const pt::BvhNode& a = actual[i];
        const pt::BvhNode& e = direct.topology.nodes[i];
        INFO("node " << i);
        REQUIRE(a.offset == e.offset);
        REQUIRE(a.count == e.count);
        REQUIRE(a.kind == e.kind);
        REQUIRE(same_bits(a.bbox.x.min, e.bbox.x.min));
        REQUIRE(same_bits(a.bbox.x.max, e.bbox.x.max));
        REQUIRE(same_bits(a.bbox.y.min, e.bbox.y.min));
        REQUIRE(same_bits(a.bbox.y.max, e.bbox.y.max));
        REQUIRE(same_bits(a.bbox.z.min, e.bbox.z.min));
        REQUIRE(same_bits(a.bbox.z.max, e.bbox.z.max));
    }
}
