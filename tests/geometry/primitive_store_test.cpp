#include "pt/core/hit_record.hpp"
#include "pt/geometry/bvh_node.hpp"
#include "pt/geometry/mesh.hpp"
#include "pt/geometry/primitive_kind.hpp"
#include "pt/geometry/primitive_store.hpp"
#include "pt/geometry/sphere.hpp"
#include "pt/materials/lambertian.hpp"
#include "pt/materials/material.hpp"
#include "pt/math/color.hpp"
#include "pt/math/constants.hpp"
#include "pt/math/interval.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include "pt/textures/solid_color.hpp"
#include "support/probe_texture.hpp"
#include "support/test_support.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <initializer_list>
#include <optional>

namespace {

using pt::Float;
using pt::HitRecord;
using pt::Interval;
using pt::Mesh;
using pt::MeshData;
using pt::Point3;
using pt::PrimitiveHit;
using pt::PrimitiveKind;
using pt::PrimitiveStore;
using pt::Ray;
using pt::Vec3;
using pt::operator""_f;
using pt_test::require_near;

const Interval visible{0.0_f, pt::infinity};

// Two materials that differ only by address: a record's material then names the mesh it was read from.
const pt::SolidColor albedo{pt::Color(0.5_f, 0.5_f, 0.5_f)};
const pt::Lambertian material_a{&albedo};
const pt::Lambertian material_b{&albedo};

// One triangle per centre, all in the plane z = 5 and three units apart along x, so a ray straight up +z
// through a centre meets that triangle and no other. Every vertex carries the same uv, which makes the
// interpolated uv that constant and a second witness of the mesh a hit was read from.
[[nodiscard]] MeshData triangles_at(std::initializer_list<Float> centres, Float uv) {
    MeshData data;
    for (const Float x : centres) {
        const auto first = static_cast<std::uint32_t>(data.positions.size());
        data.positions.emplace_back(x - 1.0_f, -1.0_f, 5.0_f);
        data.positions.emplace_back(x + 1.0_f, -1.0_f, 5.0_f);
        data.positions.emplace_back(x, 1.0_f, 5.0_f);
        data.indices.insert(data.indices.end(), {first, first + 1, first + 2});
    }
    data.uvs.assign(data.positions.size(), pt::Uv{.u = uv, .v = uv});
    return data;
}

// Every slot of the store in one triangle leaf, as a tree with a single leaf would hand it over.
[[nodiscard]] pt::BvhNode leaf_over(std::uint16_t count) {
    pt::BvhNode leaf;
    leaf.offset = 0;
    leaf.count = count;
    leaf.kind = PrimitiveKind::triangle;
    return leaf;
}

struct Expected {
    std::uint32_t slot{};
    const pt::Material* mat = nullptr;
    Float uv{};
};

// The full round trip a tree makes: the leaf scan picks the winner, then the fill reads its attributes.
void require_hit_reads(const PrimitiveStore& store, const pt::BvhNode& leaf, Float x, const Expected& expected) {
    const Ray up{Point3(x, 0, 0), Vec3(0, 0, 1)};

    PrimitiveHit hit;
    const std::optional<Float> t = store.closest_in_leaf(leaf, up, visible, hit);
    REQUIRE(t);
    REQUIRE(hit.kind == PrimitiveKind::triangle);
    REQUIRE(hit.slot == expected.slot);

    HitRecord rec;
    store.fill(hit, up, rec);
    require_near(rec.t, 5.0_f);
    REQUIRE(rec.mat == expected.mat);
    require_near(rec.u, expected.uv);
    require_near(rec.v, expected.uv);
}

} // namespace

TEST_CASE("each triangle reads its attributes from the mesh it came from", "[geometry][primitive_store]") {
    const Mesh mesh_a(triangles_at({0.0_f, 6.0_f}, 0.25_f), &material_a);
    const Mesh mesh_b(triangles_at({3.0_f}, 0.75_f), &material_b);

    PrimitiveStore store;
    const std::uint32_t a = store.add_mesh(mesh_a);
    const std::uint32_t b = store.add_mesh(mesh_b);

    // Interleaved, the way a build over two meshes lays its leaves out: no slot range belongs to one mesh,
    // so only a per-slot record can tell them apart.
    REQUIRE(store.add_triangle(a, 0) == 0U);
    REQUIRE(store.add_triangle(b, 0) == 1U);
    REQUIRE(store.add_triangle(a, 1) == 2U);

    const pt::BvhNode leaf = leaf_over(3);
    require_hit_reads(store, leaf, 0.0_f, {.slot = 0, .mat = &material_a, .uv = 0.25_f});
    require_hit_reads(store, leaf, 3.0_f, {.slot = 1, .mat = &material_b, .uv = 0.75_f});
    require_hit_reads(store, leaf, 6.0_f, {.slot = 2, .mat = &material_a, .uv = 0.25_f});
}

TEST_CASE("a mesh registered after the first triangles leaves those on their own mesh", "[geometry][primitive_store]") {
    const Mesh mesh_a(triangles_at({0.0_f}, 0.25_f), &material_a);
    const Mesh mesh_b(triangles_at({3.0_f}, 0.75_f), &material_b);

    PrimitiveStore store;
    const std::uint32_t a = store.add_mesh(mesh_a);
    REQUIRE(store.add_triangle(a, 0) == 0U);

    // The second mesh arrives after a slot exists: the store must start telling slots apart without
    // moving the one it already holds onto the newcomer.
    const std::uint32_t b = store.add_mesh(mesh_b);
    REQUIRE(store.add_triangle(b, 0) == 1U);

    const pt::BvhNode leaf = leaf_over(2);
    require_hit_reads(store, leaf, 0.0_f, {.slot = 0, .mat = &material_a, .uv = 0.25_f});
    require_hit_reads(store, leaf, 3.0_f, {.slot = 1, .mat = &material_b, .uv = 0.75_f});
}

TEST_CASE("a report replaces the whole record, its kind included", "[geometry][primitive_store]") {
    // A sphere straight ahead at nine and a triangle in front of it at five: two leaves of two kinds, met in an
    // order a traversal can meet them. The second report has to replace the first entirely - a kind left over
    // from the sphere would send the fill to the sphere's arrays with the triangle's slot and parameters.
    const pt::Sphere ball{Point3(0, 0, 10), 1.0_f, &material_b};
    const Mesh mesh(triangles_at({0.0_f}, 0.25_f), &material_a);

    PrimitiveStore store;
    REQUIRE(store.add_sphere(ball) == 0U);
    const std::uint32_t m = store.add_mesh(mesh);
    REQUIRE(store.add_triangle(m, 0) == 0U);

    pt::BvhNode sphere_leaf;
    sphere_leaf.offset = 0;
    sphere_leaf.count = 1;
    sphere_leaf.kind = PrimitiveKind::sphere;
    const pt::BvhNode triangle_leaf = leaf_over(1);

    const Ray up{Point3(0, 0, 0), Vec3(0, 0, 1)};

    PrimitiveHit hit;
    const std::optional<Float> far = store.closest_in_leaf(sphere_leaf, up, visible, hit);
    REQUIRE(far);
    REQUIRE(hit.kind == PrimitiveKind::sphere);

    const std::optional<Float> near = store.closest_in_leaf(triangle_leaf, up, Interval(0.0_f, *far), hit);
    REQUIRE(near);
    REQUIRE(hit.kind == PrimitiveKind::triangle);
    REQUIRE(hit.slot == 0U);
    require_near(hit.t, 5.0_f);

    HitRecord rec;
    store.fill(hit, up, rec);
    REQUIRE(rec.mat == &material_a);
    require_near(rec.u, 0.25_f);

    // The other order: the sphere comes second and farther, so it does not report and the triangle stays.
    PrimitiveHit kept;
    REQUIRE(store.closest_in_leaf(triangle_leaf, up, visible, kept));
    REQUIRE_FALSE(store.closest_in_leaf(sphere_leaf, up, Interval(0.0_f, kept.t), kept));
    REQUIRE(kept.kind == PrimitiveKind::triangle);
}

TEST_CASE("a sphere's coordinates are filled only for a material that reads them", "[geometry][primitive_store]") {
    // Three copies of one sphere, met at (0, 0, 9) where the outward normal is -z: u = 0.75, v = 0.5 on its map.
    const pt_test::ProbeTexture probe{pt::Color(0.5_f, 0.5_f, 0.5_f)};
    const pt::Lambertian textured{&probe};
    const pt::Sphere mapped{Point3(0, 0, 10), 1.0_f, &textured};
    const pt::Sphere plain{Point3(0, 0, 10), 1.0_f, &material_a};
    const pt::Sphere bare{Point3(0, 0, 10), 1.0_f, nullptr};

    PrimitiveStore store;
    REQUIRE(store.add_sphere(mapped) == 0U);
    REQUIRE(store.add_sphere(plain) == 1U);
    REQUIRE(store.add_sphere(bare) == 2U);

    const Ray up{Point3(0, 0, 0), Vec3(0, 0, 1)};
    const auto record_of = [&](std::uint32_t slot) {
        // Over coordinates a previous hit left behind: skipping the computation has to clear them, not keep them.
        HitRecord rec;
        rec.u = 0.125_f;
        rec.v = 0.375_f;
        store.fill(PrimitiveHit{.t = 9.0_f, .slot = slot, .kind = PrimitiveKind::sphere}, up, rec);
        return rec;
    };

    const HitRecord read = record_of(0);
    require_near(read.u, 0.75_f);
    require_near(read.v, 0.5_f);

    // A solid colour never looks: nothing is computed, and nothing stale survives.
    const HitRecord skipped = record_of(1);
    REQUIRE(skipped.u == 0.0_f);
    REQUIRE(skipped.v == 0.0_f);

    // Nothing to ask: computed, as the safe answer.
    const HitRecord unasked = record_of(2);
    require_near(unasked.u, 0.75_f);
    require_near(unasked.v, 0.5_f);
}
