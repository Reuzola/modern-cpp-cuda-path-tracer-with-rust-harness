#include "pt/core/hit_record.hpp"
#include "pt/geometry/bvh_node.hpp"
#include "pt/geometry/mesh.hpp"
#include "pt/geometry/primitive_kind.hpp"
#include "pt/geometry/primitive_store.hpp"
#include "pt/materials/lambertian.hpp"
#include "pt/materials/material.hpp"
#include "pt/math/color.hpp"
#include "pt/math/constants.hpp"
#include "pt/math/interval.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include "pt/textures/solid_color.hpp"
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
    REQUIRE(hit.triangle.slot == expected.slot);

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
