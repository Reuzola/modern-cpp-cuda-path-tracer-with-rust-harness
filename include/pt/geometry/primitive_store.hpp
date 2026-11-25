#pragma once
#include "pt/core/hit_record.hpp"
#include "pt/geometry/bvh_node.hpp"
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
#include <vector>

namespace pt {

class Material;
class Mesh;

// The nearest candidate a query has met so far, in the one shape every kind's test reports: the distance and
// the two parameters of the surface it met there. A leaf reports only a candidate nearer than every earlier one,
// and a report overwrites the whole record, kind included, so the record always names the winner whole.
struct PrimitiveHit {
    Float t{};
    // The kind's own surface coordinates, not texture coordinates: b1 and b2 for a triangle, alpha and beta
    // for a quad. A sphere reports none; its fill rebuilds everything from the point.
    Float u{};
    Float v{};
    std::uint32_t slot{}; // Into the arrays of `kind`
    PrimitiveKind kind{};
};

// Three scalars, a slot and a kind: what a query carries from report to report and hands to the fill.
static_assert(sizeof(PrimitiveHit) == 3 * sizeof(Float) + 2 * sizeof(std::uint32_t));

// The leaf scan runs in every traversal's inner loop, so it lives in the header where each tree's traversal can inline it.
namespace detail {

// How a leaf reaches one kind's stored shapes: the geometric test, and how its result becomes a report.
template <typename Shape>
struct ShapeTraits;

// A sphere's hit is its distance; the fill rebuilds the rest
template <>
struct ShapeTraits<SphereShape> {
    using Hit = Float;
    static constexpr PrimitiveKind kind = PrimitiveKind::sphere;

    [[nodiscard]] static bool intersect(const Ray& r, const Interval& ray_t, const SphereShape& shape, Hit& out) noexcept {
        const std::optional<Float> t = intersect_sphere(r, ray_t, shape);
        if (!t) return false;
        out = *t;
        return true;
    }

    [[nodiscard]] static constexpr PrimitiveHit report(const Hit& hit, std::uint32_t slot) noexcept {
        return {.t = hit, .slot = slot, .kind = kind};
    }
};

template <>
struct ShapeTraits<QuadShape> {
    using Hit = QuadHit;
    static constexpr PrimitiveKind kind = PrimitiveKind::quad;

    [[nodiscard]] static bool intersect(const Ray& r, const Interval& ray_t, const QuadShape& shape, Hit& out) noexcept {
        return intersect_quad(r, ray_t, shape, out);
    }

    [[nodiscard]] static constexpr PrimitiveHit report(const Hit& hit, std::uint32_t slot) noexcept {
        return {.t = hit.t, .u = hit.alpha, .v = hit.beta, .slot = slot, .kind = kind};
    }
};

template <>
struct ShapeTraits<TriangleCorners> {
    using Hit = TriangleHit;
    static constexpr PrimitiveKind kind = PrimitiveKind::triangle;

    [[nodiscard]] static bool intersect(const Ray& r, const Interval& ray_t, const TriangleCorners& shape, Hit& out) noexcept {
        return intersect_triangle(r, ray_t, shape.v0, shape.v1, shape.v2, out);
    }

    [[nodiscard]] static constexpr PrimitiveHit report(const Hit& hit, std::uint32_t slot) noexcept {
        return {.t = hit.t, .u = hit.b1, .v = hit.b2, .slot = slot, .kind = kind};
    }
};

// Tests one leaf's run inside `open`, narrowing as it goes. Every candidate that narrows overwrites `winner`,
// so the last write is the leaf's nearest; returns its distance, or nothing.
template <typename Shape>
[[nodiscard]] std::optional<Float> closest_in_run(std::span<const Shape> shapes, const BvhNode& leaf, const Ray& r,
                                                  const Interval& open, PrimitiveHit& winner) noexcept {
    Float closest = open.max;
    bool leaf_hit{false};

    const std::uint32_t end = leaf.offset + static_cast<std::uint32_t>(leaf.count);
    for (std::uint32_t slot = leaf.offset; slot < end; ++slot) {
        count_leaf_test();

        typename ShapeTraits<Shape>::Hit candidate{};
        if (ShapeTraits<Shape>::intersect(r, Interval(open.min, closest), shapes[slot], candidate)) {
            winner = ShapeTraits<Shape>::report(candidate, slot);
            closest = winner.t;
            leaf_hit = true;
        }
    }

    return leaf_hit ? std::optional<Float>(closest) : std::nullopt;
}

} // namespace detail

// The primitives of one tree, each kind in its own arrays and laid out in the tree's leaf order, so a leaf
// names a contiguous run of one kind. Spheres and quads are copied in with their materials; a triangle's
// corners are copied and its attributes read back from its mesh, for the candidate that wins only.
// Owns no tree: whoever holds the topology hands it the leaves of the kinds it holds.
class PrimitiveStore {
public:
    // Room for the given counts, so the appends below never reallocate.
    void reserve(std::size_t spheres, std::size_t quads, std::size_t triangles);

    // Each appends one primitive at the end of its kind's arrays and returns the slot it took there. Called in
    // leaf order, which is what makes a leaf's run contiguous. A triangle names its mesh by the index add_mesh returned.
    [[nodiscard]] std::uint32_t add_sphere(const Sphere& sphere);
    [[nodiscard]] std::uint32_t add_quad(const Quad& quad);
    [[nodiscard]] std::uint32_t add_triangle(std::uint32_t mesh, std::uint32_t triangle_index);

    // Registers a mesh whose triangles the store will hold and returns the index add_triangle takes for it.
    // The mesh must outlive the store.
    [[nodiscard]] std::uint32_t add_mesh(const Mesh& mesh);

    // Tests one leaf of a kind this store holds inside `open`, narrowing as it goes.
    // A candidate that narrows overwrites `hit`, its kind included.
    [[nodiscard]] std::optional<Float> closest_in_leaf(const BvhNode& leaf, const Ray& r, const Interval& open, PrimitiveHit& hit) const noexcept;

    // Writes the record of the candidate `hit` names. `r` must be the ray it was found with: a sphere reprojects
    // along it, and orientation is decided against it.
    void fill(const PrimitiveHit& hit, const Ray& r, HitRecord& rec) const noexcept;

private:
    // Shapes and their materials side by side, both in leaf order: the test reads the first, the fill one entry of the second.
    template <typename Shape>
    struct ShapeArrays {
        std::vector<Shape> shapes;
        std::vector<const Material*> materials;
    };

    // Corners in leaf order for the test; each slot's mesh and triangle index for the attributes of the one that wins.
    // One mesh names every slot, so the per-slot index exists only once a second mesh arrives.
    struct TriangleArrays {
        std::vector<TriangleCorners> corners;
        std::vector<std::uint32_t> ids;     // Leaf slot to the mesh's own triangle index
        std::vector<const Mesh*> meshes;    // Indexed by what add_mesh returned
        std::vector<std::uint32_t> mesh_of; // Slot to its entry in meshes; empty while there is only one
    };

    ShapeArrays<SphereShape> spheres_;
    ShapeArrays<QuadShape> quads_;
    TriangleArrays triangles_;

    void fill_triangle(const Ray& r, const PrimitiveHit& hit, HitRecord& rec) const noexcept;
};

inline std::optional<Float> PrimitiveStore::closest_in_leaf(const BvhNode& leaf, const Ray& r, const Interval& open, PrimitiveHit& hit) const noexcept {
    assert(is_primitive_kind(leaf.kind));

    std::optional<Float> t;
    switch (leaf.kind) {
    case PrimitiveKind::sphere:
        t = detail::closest_in_run<SphereShape>(spheres_.shapes, leaf, r, open, hit);
        break;
    case PrimitiveKind::quad:
        t = detail::closest_in_run<QuadShape>(quads_.shapes, leaf, r, open, hit);
        break;
    case PrimitiveKind::triangle:
        t = detail::closest_in_run<TriangleCorners>(triangles_.corners, leaf, r, open, hit);
        break;
    case PrimitiveKind::instance:
        break; // Excluded above: a leaf of children is its tree's to walk.
    }

    return t;
}

} // namespace pt
