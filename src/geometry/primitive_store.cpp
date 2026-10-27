#include "pt/geometry/primitive_store.hpp"
#include "pt/core/hit_record.hpp"
#include "pt/geometry/bvh.hpp"
#include "pt/geometry/mesh.hpp"
#include "pt/geometry/primitive_kind.hpp"
#include "pt/geometry/quad.hpp"
#include "pt/geometry/sphere.hpp"
#include "pt/geometry/triangle.hpp"
#include "pt/math/interval.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/scalar.hpp"
#include "pt/util/stats.hpp"
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace pt {

namespace {

// How a leaf reaches one kind's stored shapes: the geometric test, and the distance its hit carries.
// Specialised per stored shape; the leaf scan below is written against this and nothing else.
template <typename Shape>
struct ShapeTraits;

// A sphere's hit is its distance; the fill rebuilds the rest
template <>
struct ShapeTraits<SphereShape> {
    using Hit = Float;

    [[nodiscard]] static bool intersect(const Ray& r, const Interval& ray_t, const SphereShape& shape, Hit& out) noexcept {
        const std::optional<Float> t = intersect_sphere(r, ray_t, shape);
        if (!t) return false;
        out = *t;
        return true;
    }

    [[nodiscard]] static Float distance(const Hit& hit) noexcept { return hit; }
};

template <>
struct ShapeTraits<QuadShape> {
    using Hit = QuadHit;

    [[nodiscard]] static bool intersect(const Ray& r, const Interval& ray_t, const QuadShape& shape, Hit& out) noexcept {
        return intersect_quad(r, ray_t, shape, out);
    }

    [[nodiscard]] static Float distance(const Hit& hit) noexcept { return hit.t; }
};

template <>
struct ShapeTraits<TriangleCorners> {
    using Hit = TriangleHit;

    [[nodiscard]] static bool intersect(const Ray& r, const Interval& ray_t, const TriangleCorners& shape, Hit& out) noexcept {
        return intersect_triangle(r, ray_t, shape.v0, shape.v1, shape.v2, out);
    }

    [[nodiscard]] static Float distance(const Hit& hit) noexcept { return hit.t; }
};

// Tests one leaf's run inside `open`, narrowing as it goes. Every candidate that narrows overwrites `winner`,
// so the last write is the leaf's nearest; returns its distance, or nothing.
template <typename Shape>
[[nodiscard]] std::optional<Float> closest_in_run(std::span<const Shape> shapes, const BvhNode& leaf, const Ray& r,
                                                  const Interval& open, SlotHit<typename ShapeTraits<Shape>::Hit>& winner) noexcept {
    Float closest = open.max;
    bool leaf_hit{false};

    const std::uint32_t end = leaf.offset + static_cast<std::uint32_t>(leaf.count);
    for (std::uint32_t slot = leaf.offset; slot < end; ++slot) {
        count_leaf_test();

        typename ShapeTraits<Shape>::Hit candidate{};
        if (ShapeTraits<Shape>::intersect(r, Interval(open.min, closest), shapes[slot], candidate)) {
            closest = ShapeTraits<Shape>::distance(candidate);
            winner = {.slot = slot, .hit = candidate};
            leaf_hit = true;
        }
    }

    return leaf_hit ? std::optional<Float>(closest) : std::nullopt;
}

} // namespace

void PrimitiveStore::reserve(std::size_t spheres, std::size_t quads, std::size_t triangles) {
    spheres_.shapes.reserve(spheres);
    spheres_.materials.reserve(spheres);
    quads_.shapes.reserve(quads);
    quads_.materials.reserve(quads);
    triangles_.corners.reserve(triangles);
    triangles_.ids.reserve(triangles);
}

std::uint32_t PrimitiveStore::add_sphere(const Sphere& sphere) {
    const std::uint32_t slot = static_cast<std::uint32_t>(spheres_.shapes.size());
    spheres_.shapes.push_back(sphere.shape());
    spheres_.materials.push_back(sphere.material());
    return slot;
}

std::uint32_t PrimitiveStore::add_quad(const Quad& quad) {
    const std::uint32_t slot = static_cast<std::uint32_t>(quads_.shapes.size());
    quads_.shapes.push_back(quad.shape());
    quads_.materials.push_back(quad.material());
    return slot;
}

std::uint32_t PrimitiveStore::add_triangle(const Mesh& mesh, std::uint32_t triangle_index) {
    // One mesh per store: every slot's attributes are read back from it.
    assert(triangles_.mesh == nullptr || triangles_.mesh == &mesh);

    triangles_.mesh = &mesh;
    const std::uint32_t slot = static_cast<std::uint32_t>(triangles_.corners.size());
    const auto [a, b, c] = mesh.triangle(triangle_index);
    triangles_.corners.push_back(TriangleCorners{.v0 = a, .v1 = b, .v2 = c});
    triangles_.ids.push_back(triangle_index);
    return slot;
}

std::optional<Float> PrimitiveStore::closest_in_leaf(const BvhNode& leaf, const Ray& r, const Interval& open, PrimitiveHit& hit) const noexcept {
    assert(leaf.kind != PrimitiveKind::opaque);

    std::optional<Float> t;
    switch (leaf.kind) {
    case PrimitiveKind::sphere:
        t = closest_in_run<SphereShape>(spheres_.shapes, leaf, r, open, hit.sphere);
        break;
    case PrimitiveKind::quad:
        t = closest_in_run<QuadShape>(quads_.shapes, leaf, r, open, hit.quad);
        break;
    case PrimitiveKind::triangle:
        t = closest_in_run<TriangleCorners>(triangles_.corners, leaf, r, open, hit.triangle);
        break;
    case PrimitiveKind::opaque:
        // Excluded above: an opaque leaf is its tree's to test.
        break;
    }

    if (t) hit.kind = leaf.kind;
    return t;
}

void PrimitiveStore::fill(const PrimitiveHit& hit, const Ray& r, HitRecord& rec) const noexcept {
    assert(hit.kind != PrimitiveKind::opaque);

    switch (hit.kind) {
    case PrimitiveKind::sphere:
        fill_sphere_record(r, spheres_.shapes[hit.sphere.slot], hit.sphere.hit, spheres_.materials[hit.sphere.slot], rec);
        break;
    case PrimitiveKind::quad:
        fill_quad_record(r, quads_.shapes[hit.quad.slot], hit.quad.hit, quads_.materials[hit.quad.slot], rec);
        break;
    case PrimitiveKind::triangle:
        fill_triangle(r, hit.triangle, rec);
        break;
    case PrimitiveKind::opaque:
        // Excluded above: an opaque child fills its own record.
        break;
    }
}

void PrimitiveStore::fill_triangle(const Ray& r, const SlotHit<TriangleHit>& winner, HitRecord& rec) const noexcept {
    const TriangleCorners& tri = triangles_.corners[winner.slot];
    const std::uint32_t id = triangles_.ids[winner.slot];
    const Uv uv = triangles_.mesh->texture_uv(id, winner.hit.b1, winner.hit.b2);

    rec.t = winner.hit.t;
    rec.p = barycentric_point(tri.v0, tri.v1, tri.v2, winner.hit.b1, winner.hit.b2);
    rec.spawn_margin = 0.0_f; // Written explicitly: the caller reuses records across queries.
    rec.u = uv.u;
    rec.v = uv.v;
    rec.mat = triangles_.mesh->material();
    rec.set_face_normal(r, winner.hit.normal, triangles_.mesh->shading_normal(id, winner.hit.b1, winner.hit.b2, winner.hit.normal));
}

} // namespace pt
