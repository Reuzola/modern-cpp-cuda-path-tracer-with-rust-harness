#include "pt/core/hit_record.hpp"
#include "pt/core/hittable.hpp"
#include "pt/geometry/box.hpp"
#include "pt/geometry/bvh.hpp"
#include "pt/geometry/instance.hpp"
#include "pt/geometry/level.hpp"
#include "pt/geometry/mesh.hpp"
#include "pt/geometry/quad.hpp"
#include "pt/geometry/scene_graph.hpp"
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
#include "pt/util/overloaded.hpp"
#include "support/placed_reference.hpp"
#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

namespace {

using pt::BoxFaces;
using pt::HitRecord;
using pt::Hittable;
using pt::Instance;
using pt::Interval;
using pt::LevelItem;
using pt::LevelPlan;
using pt::Mesh;
using pt::Point3;
using pt::Ray;
using pt::Sampler;
using pt::SceneGraph;
using pt::SceneNode;
using pt::Transform;
using pt::Vec3;
using pt::operator""_f;
using pt_test::Placed;
using pt_test::same_vec;

const Interval visible{0.0_f, pt::infinity};

// Objects for graphs to place, each with a material of its own so a record names the object it came from.
// A mesh is also kept as standalone triangles, which is how the reference scan sees it. Immovable: the
// graphs and trees built from them point at the objects.
class Objects {
public:
    Objects() = default;
    Objects(const Objects&) = delete;
    Objects& operator=(const Objects&) = delete;

    const pt::Sphere* sphere(Sampler& sampler) {
        const Point3 centre{sampler.next_scalar(-8.0_f, 8.0_f), sampler.next_scalar(-8.0_f, 8.0_f), sampler.next_scalar(-8.0_f, 8.0_f)};
        return objects_.create<pt::Sphere>(centre, sampler.next_scalar(0.3_f, 1.5_f), new_material());
    }

    const pt::Quad* quad(Sampler& sampler) {
        const Point3 corner{sampler.next_scalar(-8.0_f, 8.0_f), sampler.next_scalar(-8.0_f, 8.0_f), sampler.next_scalar(-8.0_f, 8.0_f)};
        return objects_.create<pt::Quad>(corner, Vec3::random(-3.0_f, 3.0_f, sampler), Vec3::random(-3.0_f, 3.0_f, sampler), new_material());
    }

    BoxFaces box(Sampler& sampler) {
        const Point3 a{sampler.next_scalar(-6.0_f, 0.0_f), sampler.next_scalar(-6.0_f, 0.0_f), sampler.next_scalar(-6.0_f, 0.0_f)};
        const Point3 b = a + Vec3(sampler.next_scalar(1.0_f, 5.0_f), sampler.next_scalar(1.0_f, 5.0_f), sampler.next_scalar(1.0_f, 5.0_f));
        return pt::box(objects_, a, b, new_material());
    }

    const Mesh* mesh(Sampler& sampler, int triangles) {
        const pt::Material* material = new_material();
        meshes_.push_back(std::make_unique<Mesh>(pt_test::random_triangles(sampler, triangles), material));
        const Mesh* mesh = meshes_.back().get();

        std::vector<const Hittable*>& standalone = triangles_[mesh];
        for (std::size_t i = 0; i < mesh->triangle_count(); ++i) {
            const auto [a, b, c] = mesh->triangle(i);
            standalone.push_back(objects_.create<pt::Triangle>(a, b, c, material));
        }
        return mesh;
    }

    const Mesh* empty_mesh() {
        meshes_.push_back(std::make_unique<Mesh>(pt::MeshData{}, new_material()));
        const Mesh* mesh = meshes_.back().get();

        // Known to triangles_of like any other mesh, with nothing to show for it.
        triangles_.try_emplace(mesh);
        return mesh;
    }

    [[nodiscard]] const std::vector<const Hittable*>& triangles_of(const Mesh* mesh) const { return triangles_.at(mesh); }

private:
    [[nodiscard]] const pt::Material* new_material() { return materials_.create<pt::Lambertian>(&albedo_); }

    const pt::SolidColor albedo_{pt::Color(0.5_f, 0.5_f, 0.5_f)};
    pt::Arena<pt::Material> materials_;
    pt::Arena<Hittable> objects_;
    std::vector<std::unique_ptr<Mesh>> meshes_;
    std::map<const Mesh*, std::vector<const Hittable*>> triangles_;
};

// One primitive under a node, and what places it relative to that node's space: nothing, for one in the
// node's own space.
struct Placement {
    const Hittable* object = nullptr;
    std::optional<Transform> transform;
};

// Walks the graph on its own, composing bottom up - a transform's matrix times whatever its child already
// carries - which is the association flattening uses. Matrix products round, so the reference must group
// them the same way to be compared bit for bit; what it must not share is how levels are planned.
[[nodiscard]] std::vector<Placement> placements(const Objects& objects, const SceneNode& node) {
    std::vector<Placement> out;
    // clang-format off
    std::visit(pt::Overloaded{
        [&](const pt::Sphere* sphere) { out.push_back({.object = sphere, .transform = std::nullopt}); },
        [&](const pt::Quad* quad)     { out.push_back({.object = quad, .transform = std::nullopt}); },
        [&](const BoxFaces& box) {
            for (const pt::Quad* face : box.faces)
                out.push_back({.object = face, .transform = std::nullopt});
        },
        [&](const Mesh* mesh) {
            for (const Hittable* triangle : objects.triangles_of(mesh))
                out.push_back({.object = triangle, .transform = std::nullopt});
        },
        [&](const pt::GroupNode* group) {
            for (const SceneNode& child : group->children) {
                for (const Placement& p : placements(objects, child))
                    out.push_back(p);
            }
        },
        [&](const pt::TransformNode* transform) {
            for (const Placement& p : placements(objects, transform->child)) {
                const Transform composed = p.transform ? transform->transform * *p.transform : transform->transform;
                out.push_back({.object = p.object, .transform = composed});
            }
        },
    }, node);
    // clang-format on
    return out;
}

[[nodiscard]] std::vector<Placed> reference_of(const Objects& objects, const std::vector<SceneNode>& roots) {
    std::vector<Placed> placed;
    for (const SceneNode& root : roots) {
        for (const Placement& p : placements(objects, root)) {
            // The index an instance names a tree by plays no part in the scan.
            std::optional<Instance> instance;
            if (p.transform) instance = Instance(*p.transform, 0);
            placed.push_back(Placed{.object = p.object, .instance = instance});
        }
    }
    return placed;
}

template <typename T>
[[nodiscard]] bool is_item(const LevelItem& item, const T* expected) {
    const T* const* held = std::get_if<const T*>(&item);
    return held != nullptr && *held == expected;
}

[[nodiscard]] const Instance& instance_at(const LevelItem& item) {
    const Instance* instance = std::get_if<Instance>(&item);
    REQUIRE(instance != nullptr);
    return *instance;
}

// Two transforms agree bit for bit where it matters: on a point carried out, and on one carried back.
void require_same_transform(const Transform& actual, const Transform& expected) {
    const Point3 probe(1.25_f, -2.5_f, 3.75_f);
    REQUIRE(same_vec(actual.apply_point(probe), expected.apply_point(probe)));
    REQUIRE(same_vec(actual.apply_inverse_point(probe), expected.apply_inverse_point(probe)));
}

const Transform turn = Transform::translation(Vec3(12, 0, 0)) * Transform::rotation_y(40.0_f);
const Transform lift = Transform::translation(Vec3(0, 9, 0)) * Transform::scaling(Vec3(1.5_f, 0.5_f, 1.0_f));

} // namespace

TEST_CASE("a group dissolves into its level in the order it was given", "[geometry][scene_graph]") {
    Objects objects;
    SceneGraph graph;
    Sampler sampler{pt::sampler_seed(51, 0, 0)};

    const pt::Sphere* s1 = objects.sphere(sampler);
    const pt::Sphere* s2 = objects.sphere(sampler);
    const pt::Quad* q1 = objects.quad(sampler);
    const pt::Quad* q2 = objects.quad(sampler);

    const pt::GroupNode* inner = graph.add_group({s2});
    const pt::GroupNode* outer = graph.add_group({q1, inner});
    const std::vector<SceneNode> roots{s1, outer, q2};

    const LevelPlan plan = pt::plan_levels(roots);

    // No transform, so no bottom level: the groups were only ever organisation. The order is the depth-first
    // order of the graph, which is what a builder's ties are broken by.
    REQUIRE(plan.bottoms.empty());
    REQUIRE(plan.top.items.size() == 4);
    REQUIRE(is_item(plan.top.items[0], s1));
    REQUIRE(is_item(plan.top.items[1], q1));
    REQUIRE(is_item(plan.top.items[2], s2));
    REQUIRE(is_item(plan.top.items[3], q2));
}

TEST_CASE("every transform of one node shares that node's bottom level", "[geometry][scene_graph]") {
    Objects objects;
    SceneGraph graph;
    Sampler sampler{pt::sampler_seed(52, 0, 0)};

    const Mesh* mesh = objects.mesh(sampler, 16);
    const std::vector<SceneNode> roots{graph.add_transform(turn, mesh), graph.add_transform(lift, mesh)};

    const LevelPlan plan = pt::plan_levels(roots);

    // Planned once, placed twice: the instancing the scene format promises.
    REQUIRE(plan.bottoms.size() == 1);
    REQUIRE(plan.bottoms[0].items.size() == 1);
    REQUIRE(is_item(plan.bottoms[0].items[0], mesh));

    REQUIRE(plan.top.items.size() == 2);
    REQUIRE(instance_at(plan.top.items[0]).bvh() == 0);
    REQUIRE(instance_at(plan.top.items[1]).bvh() == 0);
    require_same_transform(instance_at(plan.top.items[0]).transform(), turn);
    require_same_transform(instance_at(plan.top.items[1]).transform(), lift);
}

TEST_CASE("a transform under a transform is composed and lifted to the top", "[geometry][scene_graph]") {
    Objects objects;
    SceneGraph graph;
    Sampler sampler{pt::sampler_seed(53, 0, 0)};

    const pt::Sphere* sphere = objects.sphere(sampler);
    const pt::Quad* quad = objects.quad(sampler);

    const pt::TransformNode* inner = graph.add_transform(lift, quad);
    const std::vector<SceneNode> roots{graph.add_transform(turn, graph.add_group({sphere, inner}))};

    const LevelPlan plan = pt::plan_levels(roots);

    // Two bottom levels, neither holding an instance: the sphere is the outer transform's own geometry, the
    // quad the inner one's. The inner placement reaches the top as one instance carrying both matrices.
    REQUIRE(plan.bottoms.size() == 2);
    REQUIRE(plan.top.items.size() == 2);

    const Instance& own = instance_at(plan.top.items[0]);
    REQUIRE(plan.bottoms[own.bvh()].items.size() == 1);
    REQUIRE(is_item(plan.bottoms[own.bvh()].items[0], sphere));
    require_same_transform(own.transform(), turn);

    const Instance& lifted = instance_at(plan.top.items[1]);
    REQUIRE(plan.bottoms[lifted.bvh()].items.size() == 1);
    REQUIRE(is_item(plan.bottoms[lifted.bvh()].items[0], quad));
    require_same_transform(lifted.transform(), turn * lift);
}

TEST_CASE("a subtree of nothing but transforms plans no level of its own", "[geometry][scene_graph]") {
    Objects objects;
    SceneGraph graph;
    Sampler sampler{pt::sampler_seed(54, 0, 0)};

    const pt::Sphere* sphere = objects.sphere(sampler);
    const pt::GroupNode* only_transforms = graph.add_group({graph.add_transform(lift, sphere)});
    const std::vector<SceneNode> roots{graph.add_transform(turn, only_transforms)};

    const LevelPlan plan = pt::plan_levels(roots);

    // An empty tree would have no box to bound an instance by. The group holds no geometry of its own,
    // so it plans no level: only the sphere's, placed once by both matrices together.
    REQUIRE(plan.bottoms.size() == 1);
    REQUIRE(plan.top.items.size() == 1);
    require_same_transform(instance_at(plan.top.items[0]).transform(), turn * lift);
}

TEST_CASE("an empty mesh places nothing anywhere", "[geometry][scene_graph]") {
    Objects objects;
    SceneGraph graph;

    const Mesh* empty = objects.empty_mesh();
    const std::vector<SceneNode> roots{empty, graph.add_transform(turn, empty)};

    const LevelPlan plan = pt::plan_levels(roots);

    REQUIRE(plan.bottoms.empty());
    REQUIRE(plan.top.items.empty());

    // And an empty plan builds the empty world, which every query misses.
    pt::BvhStats stats;
    const pt::TopLevelBvh world = pt::build_levels(plan, &stats);
    HitRecord rec;
    REQUIRE_FALSE(world.hit(Ray(Point3(0, 0, -20), Vec3(0, 0, 1)), visible, rec));
    REQUIRE(stats.bvh_count == 0);
}

TEST_CASE("a level of one box is built as one leaf, a box among others joins its level", "[geometry][scene_graph]") {
    Objects objects;
    SceneGraph graph;
    Sampler sampler{pt::sampler_seed(55, 0, 0)};

    const BoxFaces crate = objects.box(sampler);
    const pt::Quad* floor = objects.quad(sampler);

    SECTION("a box placed on its own") {
        const std::vector<SceneNode> roots{graph.add_transform(turn, crate)};
        const LevelPlan plan = pt::plan_levels(roots);

        REQUIRE(plan.bottoms.size() == 1);
        REQUIRE(plan.bottoms[0].lone_box);

        // The six faces tested in a row, as a box under a transform always was.
        const pt::TopLevelBvh world = pt::build_levels(plan);
        const pt::BvhTopology& tree = world.bottom_levels()[0].topology();
        REQUIRE(tree.nodes.size() == 1);
        REQUIRE(tree.nodes[0].count == 6);
    }

    SECTION("a box alone at the top, as a medium's boundary is") {
        const std::vector<SceneNode> roots{crate};
        REQUIRE(pt::plan_levels(roots).top.lone_box);
    }

    SECTION("a box beside other geometry") {
        const std::vector<SceneNode> roots{crate, floor};
        const LevelPlan plan = pt::plan_levels(roots);

        // Seven quads in one level, built like any other: the faces are just quads there.
        REQUIRE_FALSE(plan.top.lone_box);
        REQUIRE(plan.top.items.size() == 7);
    }

    SECTION("a box inside a group under a transform") {
        const std::vector<SceneNode> roots{graph.add_transform(turn, graph.add_group({crate}))};
        const LevelPlan plan = pt::plan_levels(roots);

        // The rule is about what a transform places, not about what a level ends up holding: a group was
        // always built as a group.
        REQUIRE(plan.bottoms.size() == 1);
        REQUIRE_FALSE(plan.bottoms[0].lone_box);
    }
}

TEST_CASE("building a plan records every tree it builds", "[geometry][scene_graph]") {
    Objects objects;
    SceneGraph graph;
    Sampler sampler{pt::sampler_seed(56, 0, 0)};

    const Mesh* mesh = objects.mesh(sampler, 16);
    const std::vector<SceneNode> roots{objects.sphere(sampler), graph.add_transform(turn, mesh), graph.add_transform(lift, mesh)};

    pt::BvhStats stats;
    const pt::TopLevelBvh world = pt::build_levels(pt::plan_levels(roots), &stats);

    // One bottom level for the shared mesh, one top level: two trees, however many placements.
    REQUIRE(stats.bvh_count == 2);
    REQUIRE(world.bottom_levels().size() == 1);
    REQUIRE(world.instance_count() == 2);
}

TEST_CASE("a flattened graph answers exactly like a scan of everything it places", "[geometry][scene_graph]") {
    Objects objects;
    SceneGraph graph;
    Sampler sampler{pt::sampler_seed(57, 0, 0)};

    // A cluster with geometry of its own and a transform inside it, placed three times - once through two
    // transforms - so its bottom level is shared and its inner box is lifted and composed every time.
    std::vector<SceneNode> cluster_children;
    cluster_children.reserve(10);
    for (int i = 0; i < 8; ++i)
        cluster_children.emplace_back(objects.sphere(sampler));
    cluster_children.emplace_back(objects.mesh(sampler, 20));
    cluster_children.emplace_back(graph.add_transform(Transform::rotation_x(25.0_f), objects.box(sampler)));
    const pt::GroupNode* cluster = graph.add_group(std::move(cluster_children));

    std::vector<SceneNode> roots;
    roots.reserve(22);
    for (int i = 0; i < 15; ++i)
        roots.emplace_back(objects.sphere(sampler));
    roots.emplace_back(graph.add_transform(turn, cluster));
    roots.emplace_back(objects.box(sampler));
    roots.emplace_back(graph.add_transform(lift, cluster));
    roots.emplace_back(objects.mesh(sampler, 20));
    roots.emplace_back(graph.add_transform(Transform::translation(Vec3(-12, -6, 3)),
                                           graph.add_transform(Transform::scaling(Vec3(-1, 1, 1)), cluster)));
    roots.emplace_back(graph.add_group({graph.add_group({objects.sphere(sampler), objects.quad(sampler)})}));
    roots.emplace_back(graph.add_transform(Transform::translation(Vec3(0, -12, 0)),
                                           graph.add_group({graph.add_transform(lift, objects.quad(sampler))})));

    const pt::TopLevelBvh world = pt::build_levels(pt::plan_levels(roots));

    // The cluster, its box and the lone quad: three trees. The cluster's three placements each bring its own
    // instance and its box's, and the last root adds one more.
    REQUIRE(world.bottom_levels().size() == 3);
    REQUIRE(world.instance_count() == 7);

    const std::vector<Placed> reference = reference_of(objects, roots);
    const pt::Aabb bounds = world.bounding_box();
    int hits = 0;

    const auto require_same_answer = [&](const Ray& r) {
        HitRecord rec;
        const bool hit = world.hit(r, visible, rec);
        const std::optional<pt_test::ReferenceHit> expected = pt_test::reference_hit(reference, r, visible);

        INFO("origin (" << r.origin().x() << ", " << r.origin().y() << ", " << r.origin().z() << ")  direction ("
                        << r.direction().x() << ", " << r.direction().y() << ", " << r.direction().z() << ")");
        REQUIRE(hit == expected.has_value());
        if (!expected) return;
        REQUIRE(pt_test::same_record(rec, expected->rec));
        ++hits;
    };

    for (int i = 0; i < 512; ++i)
        require_same_answer(pt_test::ray_into(sampler, bounds));
    for (int i = 0; i < 512; ++i)
        require_same_answer(pt_test::ray_inside(sampler, bounds));

    REQUIRE(hits > 0);
}
