#include "pt/core/hit_record.hpp"
#include "pt/core/hittable.hpp"
#include "pt/geometry/bvh.hpp"
#include "pt/geometry/mesh.hpp"
#include "pt/geometry/triangle.hpp"
#include "pt/geometry/triangle_bvh.hpp"
#include "pt/materials/lambertian.hpp"
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
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>

namespace {

using pt::Aabb;
using pt::BvhBuildSettings;
using pt::Float;
using pt::HitRecord;
using pt::Interval;
using pt::Mesh;
using pt::MeshData;
using pt::Point3;
using pt::Ray;
using pt::Sampler;
using pt::TriangleBvh;
using pt::Uv;
using pt::Vec3;
using pt::operator""_f;

const Interval visible{0.0_f, pt::infinity};

// Rays per family. The defects this suite hunts - wrong corners gathered, a slot taken for an id, a lost
// candidate filled in - show on almost every hit, so a few hundred rays find them; every extra ray only
// pays for another pass of the brute-force reference.
constexpr int rays_per_family = 256;

const pt::SolidColor albedo{pt::Color(0.5_f, 0.5_f, 0.5_f)};
const pt::Lambertian material{&albedo};

// A wavy grid of shared vertices, so neighbouring triangles' boxes overlap; with vertex normals that
// differ from the flat ones, so interpolation is visible; and with UVs, so every attribute path runs.
[[nodiscard]] MeshData wavy_grid(int cells) {
    MeshData data;
    const int n = cells + 1;
    const Float step = 4.0_f / static_cast<Float>(cells);

    for (int j = 0; j < n; ++j) {
        for (int i = 0; i < n; ++i) {
            const Float x = -2.0_f + step * static_cast<Float>(i);
            const Float y = -2.0_f + step * static_cast<Float>(j);
            data.positions.emplace_back(x, y, 0.3_f * std::sin(2.0_f * x) * std::cos(2.0_f * y));

            // The surface z = f(x, y) has normal (-df/dx, -df/dy, 1).
            const Float dzdx = 0.6_f * std::cos(2.0_f * x) * std::cos(2.0_f * y);
            const Float dzdy = -0.6_f * std::sin(2.0_f * x) * std::sin(2.0_f * y);
            data.normals.push_back(pt::unit_vector(Vec3(-dzdx, -dzdy, 1.0_f)));

            data.uvs.push_back(Uv{.u = static_cast<Float>(i) / static_cast<Float>(cells),
                                  .v = static_cast<Float>(j) / static_cast<Float>(cells)});
        }
    }

    for (int j = 0; j < cells; ++j) {
        for (int i = 0; i < cells; ++i) {
            const auto v00 = static_cast<std::uint32_t>(j * n + i);
            const auto v10 = v00 + 1;
            const auto v01 = v00 + static_cast<std::uint32_t>(n);
            const auto v11 = v01 + 1;
            data.indices.insert(data.indices.end(), {v00, v10, v11, v00, v11, v01});
        }
    }
    return data;
}

[[nodiscard]] Float bounding_radius(const Aabb& bounds) {
    return 0.5_f * Vec3(bounds.x.size(), bounds.y.size(), bounds.z.size()).length();
}

[[nodiscard]] Ray ray_into(Sampler& sampler, const Aabb& bounds) {
    const Point3 origin = bounds.centroid() + 2.0_f * bounding_radius(bounds) * pt::random_unit_vector(sampler);
    const Point3 target(sampler.next_scalar(bounds.x.min, bounds.x.max), sampler.next_scalar(bounds.y.min, bounds.y.max),
                        sampler.next_scalar(bounds.z.min, bounds.z.max));
    return Ray(origin, target - origin);
}

[[nodiscard]] Ray ray_inside(Sampler& sampler, const Aabb& bounds) {
    const Point3 origin(sampler.next_scalar(bounds.x.min, bounds.x.max), sampler.next_scalar(bounds.y.min, bounds.y.max),
                        sampler.next_scalar(bounds.z.min, bounds.z.max));
    return Ray(origin, pt::random_unit_vector(sampler));
}

// The reference: every triangle, read through the mesh's own index buffer, nearest one filled in. It
// shares nothing with the tree but the intersection test and Mesh's attribute lookups, so a tree that
// gathered the wrong corners, kept the wrong id or filled from the wrong candidate disagrees with it.
bool brute_force_hit(const Mesh& mesh, const Ray& r, const Interval& ray_t, HitRecord& rec) {
    bool found = false;
    std::size_t nearest = 0;
    pt::TriangleHit best;
    Float closest = ray_t.max;

    for (std::size_t i = 0; i < mesh.triangle_count(); ++i) {
        const auto [a, b, c] = mesh.triangle(i);
        pt::TriangleHit candidate;
        if (pt::intersect_triangle(r, Interval(ray_t.min, closest), a, b, c, candidate)) {
            found = true;
            nearest = i;
            best = candidate;
            closest = candidate.t;
        }
    }
    if (!found) return false;

    const auto [pa, pb, pc] = mesh.triangle(nearest);
    const Uv uv = mesh.texture_uv(nearest, best.b1, best.b2);
    rec.t = best.t;
    rec.p = pt::barycentric_point(pa, pb, pc, best.b1, best.b2);
    rec.spawn_margin = 0.0_f;
    rec.u = uv.u;
    rec.v = uv.v;
    rec.mat = mesh.material();
    rec.set_face_normal(r, best.normal, mesh.shading_normal(nearest, best.b1, best.b2, best.normal));
    return true;
}

// A record filled with values no hit produces: a field the tree forgets to write keeps one of these
// and fails the comparison, where a default-constructed record would hide it behind a matching zero.
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

// The first field the tree got wrong, or nullptr when the records agree. Exact throughout: the same
// intersection test on the same corner values, and the same attribute lookups on the same triangle.
// Only the order triangles are asked in differs, and that decides nothing except at exactly equal
// distances, which random rays do not produce.
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

// Compares one ray and reports whether the reference hit. The message is built only on failure: an
// INFO per ray and a REQUIRE per field cost more than the traversal they check.
bool require_same_hit(const TriangleBvh& tree, const Mesh& mesh, const Ray& r, const Interval& ray_t) {
    HitRecord tree_rec = poisoned_record();
    HitRecord reference_rec;

    const bool tree_hit = tree.hit(r, ray_t, tree_rec);
    const bool reference_hit = brute_force_hit(mesh, r, ray_t, reference_rec);

    const char* const field =
        tree_hit != reference_hit ? "hit" : (reference_hit ? first_difference(tree_rec, reference_rec) : nullptr);

    if (field != nullptr) {
        FAIL("tree and scan disagree on `" << field << "` for origin (" << r.origin().x() << ", " << r.origin().y()
                                           << ", " << r.origin().z() << ")  direction (" << r.direction().x() << ", "
                                           << r.direction().y() << ", " << r.direction().z() << ")  interval ["
                                           << ray_t.min << ", " << ray_t.max << "]");
    }
    return reference_hit;
}

void require_tree_matches_mesh(const Mesh& mesh, const BvhBuildSettings& settings, std::uint64_t seed) {
    const TriangleBvh tree(mesh, settings);
    const Aabb bounds = tree.bounding_box();
    Sampler sampler{pt::sampler_seed(seed, 0, 0)};

    // Each family has to hit something, or every comparison in it is a miss against a miss and proves nothing.
    int hits = 0;
    for (int i = 0; i < rays_per_family; ++i)
        hits += require_same_hit(tree, mesh, ray_into(sampler, bounds), visible) ? 1 : 0;
    REQUIRE(hits > 0);

    hits = 0;
    for (int i = 0; i < rays_per_family; ++i)
        hits += require_same_hit(tree, mesh, ray_inside(sampler, bounds), visible) ? 1 : 0;
    REQUIRE(hits > 0);

    hits = 0;
    for (int i = 0; i < rays_per_family; ++i) {
        // Two statements, not two arguments: argument evaluation order is unspecified, and both draw from the sampler.
        const Ray r = ray_into(sampler, bounds);
        const Interval ray_t(0.001_f, sampler.next_scalar(0.5_f, 10.0_f));
        hits += require_same_hit(tree, mesh, r, ray_t) ? 1 : 0;
    }
    REQUIRE(hits > 0);
}

} // namespace

TEST_CASE("a triangle tree answers exactly like a scan of its mesh", "[geometry][triangle_bvh]") {
    // Build settings change only the tree's shape, so they run on the mesh that exercises every attribute
    // path; the loaded meshes bring the loader's data, which does not depend on the shape.
    SECTION("a procedural mesh with vertex normals and UVs, under every build path") {
        const BvhBuildSettings settings =
            GENERATE(BvhBuildSettings{}, BvhBuildSettings{.max_leaf_size = 1}, BvhBuildSettings{.depth_limit = 1});
        const Mesh mesh(wavy_grid(16), &material);
        require_tree_matches_mesh(mesh, settings, 41);
    }

    SECTION("loaded meshes, which carry whatever attributes their files hold") {
        const std::string filename = GENERATE("unit_cube.obj", "tetrahedron.obj", "icosphere.obj");
        const Mesh mesh(pt::load_obj(std::filesystem::path(PT_ASSETS_DIR) / filename), &material);
        require_tree_matches_mesh(mesh, BvhBuildSettings{}, 43);
    }
}

TEST_CASE("a triangle tree's bounds are the union of its triangles", "[geometry][triangle_bvh]") {
    const Mesh mesh(wavy_grid(8), &material);
    const TriangleBvh tree(mesh);

    Aabb expected;
    for (std::size_t i = 0; i < mesh.triangle_count(); ++i) {
        const auto [a, b, c] = mesh.triangle(i);
        expected = Aabb(expected, pt::triangle_bounds(a, b, c));
    }

    // Exact: a union is min and max, which round nothing and do not depend on the order taken.
    const Aabb actual = tree.bounding_box();
    REQUIRE(actual.x.min == expected.x.min);
    REQUIRE(actual.x.max == expected.x.max);
    REQUIRE(actual.y.min == expected.y.min);
    REQUIRE(actual.y.max == expected.y.max);
    REQUIRE(actual.z.min == expected.z.min);
    REQUIRE(actual.z.max == expected.z.max);
}

TEST_CASE("a miss leaves the caller's record untouched", "[geometry][triangle_bvh]") {
    const Mesh mesh(wavy_grid(8), &material);
    const TriangleBvh tree(mesh);

    // The record is written after traversal, from the winner; a miss has no winner, so nothing at all
    // may be written - including by leaves whose boxes the ray entered without hitting a triangle.
    HitRecord rec = poisoned_record();
    REQUIRE_FALSE(tree.hit(Ray(Point3(0.0_f, 0.0_f, 5.0_f), Vec3(0.0_f, 0.0_f, 1.0_f)), visible, rec));
    REQUIRE(rec.t == -7.0_f);
    REQUIRE(rec.spawn_margin == 0.5_f);

    // Aimed down at the grid and stopped just short of it. Under (0.1, 0.2) lies the triangle
    // (0,0,0), (0.5,0.5,0.136), (0,0.5,0): its box, and every box above it, reaches z = 0.136 and is
    // entered by t = 4.864, while the triangle itself crosses the ray at z = 0.027, t = 4.973. So
    // leaves run inside [0, 4.9] and none finds a winner. The point stays off the grid lines, where a
    // vertical ray would lie exactly on slab planes.
    REQUIRE_FALSE(tree.hit(Ray(Point3(0.1_f, 0.2_f, 5.0_f), Vec3(0.0_f, 0.0_f, -1.0_f)), Interval(0.0_f, 4.9_f), rec));
    REQUIRE(rec.t == -7.0_f);
    REQUIRE(rec.spawn_margin == 0.5_f);
}

TEST_CASE("an empty mesh builds an empty tree", "[geometry][triangle_bvh]") {
    const Mesh mesh(MeshData{}, nullptr);
    const TriangleBvh tree(mesh);

    REQUIRE(tree.topology().nodes.empty());
    REQUIRE(tree.bounding_box().x.size() < 0.0_f);

    HitRecord rec;
    REQUIRE_FALSE(tree.hit(Ray(Point3(0.0_f, 0.0_f, 0.0_f), Vec3(1.0_f, 0.0_f, 0.0_f)), visible, rec));
}

TEST_CASE("make_triangle_bvh folds its tree into the shared stats", "[geometry][triangle_bvh]") {
    const Mesh mesh(wavy_grid(8), &material);
    pt::Arena<pt::Hittable> arena;
    pt::BvhStats stats;

    const TriangleBvh* first = pt::make_triangle_bvh(arena, mesh, &stats);
    REQUIRE(stats.bvh_count == 1);
    REQUIRE(stats.node_count == first->topology().nodes.size());
    REQUIRE(stats.leaf_count == first->topology().leaf_count);
    REQUIRE(stats.max_depth == first->topology().max_depth);

    // Totals across trees, as a scene with several meshes reports them.
    const TriangleBvh* second = pt::make_triangle_bvh(arena, mesh, &stats, BvhBuildSettings{.max_leaf_size = 1});
    REQUIRE(stats.bvh_count == 2);
    REQUIRE(stats.node_count == first->topology().nodes.size() + second->topology().nodes.size());
    REQUIRE(stats.leaf_count == first->topology().leaf_count + second->topology().leaf_count);

    REQUIRE(pt::make_triangle_bvh(arena, mesh) != nullptr);
}
