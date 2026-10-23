#pragma once
#include "pt/core/hit_record.hpp"
#include "pt/core/hittable.hpp"
#include "pt/geometry/bvh.hpp"
#include "pt/geometry/primitive_kind.hpp"
#include "pt/geometry/quad.hpp"
#include "pt/geometry/sphere.hpp"
#include "pt/math/aabb.hpp"
#include "pt/math/interval.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/scalar.hpp"
#include "pt/util/arena.hpp"
#include <chrono>
#include <concepts>
#include <optional>
#include <span>
#include <type_traits>
#include <vector>

namespace pt {

// How a set reaches one primitive kind: its shape, its hit, and the free functions over them.
// Specialised per kind; the set itself is written against this and nothing else.
template <typename Primitive>
struct ShapeTraits;

template <>
struct ShapeTraits<Sphere> {
    static constexpr PrimitiveKind kind = PrimitiveKind::sphere;

    using Shape = SphereShape;
    using Hit = Float; // A sphere's hit is its distance; the fill rebuilds the rest

    [[nodiscard]] static Aabb bounds(const Shape& shape) noexcept { return sphere_bounds(shape); }

    [[nodiscard]] static bool intersect(const Ray& r, const Interval& ray_t, const Shape& shape, Hit& out) noexcept {
        const std::optional<Float> t = intersect_sphere(r, ray_t, shape);
        if (!t) return false;
        out = *t;
        return true;
    }

    [[nodiscard]] static Float distance(const Hit& hit) noexcept { return hit; }

    static void fill(const Ray& r, const Shape& shape, const Hit& hit, const Material* mat, HitRecord& rec) noexcept {
        fill_sphere_record(r, shape, hit, mat, rec);
    }
};

template <>
struct ShapeTraits<Quad> {
    static constexpr PrimitiveKind kind = PrimitiveKind::quad;

    using Shape = QuadShape;
    using Hit = QuadHit;

    [[nodiscard]] static Aabb bounds(const Shape& shape) noexcept { return quad_bounds(shape); }

    [[nodiscard]] static bool intersect(const Ray& r, const Interval& ray_t, const Shape& shape, Hit& out) noexcept {
        return intersect_quad(r, ray_t, shape, out);
    }

    [[nodiscard]] static Float distance(const Hit& hit) noexcept { return hit.t; }

    static void fill(const Ray& r, const Shape& shape, const Hit& hit, const Material* mat, HitRecord& rec) noexcept {
        fill_quad_record(r, shape, hit, mat, rec);
    }
};

// What a kind must offer to live in a set. Trivially copyable shapes are part of it: a set
// copies them into leaf order, and a device buffer will hold them as they are.
template <typename Primitive>
concept SetPrimitive = requires(const Primitive& primitive, const Ray& r, const Interval& ray_t,
                                const typename ShapeTraits<Primitive>::Shape& shape,
                                typename ShapeTraits<Primitive>::Hit& hit, HitRecord& rec) {
    requires std::is_trivially_copyable_v<typename ShapeTraits<Primitive>::Shape>;
    { primitive.shape() } -> std::same_as<const typename ShapeTraits<Primitive>::Shape&>;
    { primitive.material() } -> std::same_as<const Material*>;
    { ShapeTraits<Primitive>::bounds(shape) } -> std::same_as<Aabb>;
    { ShapeTraits<Primitive>::intersect(r, ray_t, shape, hit) } -> std::same_as<bool>;
    { ShapeTraits<Primitive>::distance(hit) } -> std::same_as<Float>;
    { ShapeTraits<Primitive>::fill(r, shape, hit, primitive.material(), rec) } -> std::same_as<void>;
};

// Primitives of one kind under a tree of their own, as a mesh's triangles are in TriangleBvh. Shapes
// are copied into leaf order, so a leaf's test reads one contiguous run and no per-primitive object;
// materials sit beside them and are read for the winner only. Nothing points back at the objects the
// set was built from: it holds everything a query needs.
template <SetPrimitive Primitive>
class ShapeBvh final : public Hittable {
public:
    using Traits = ShapeTraits<Primitive>;
    using Shape = typename Traits::Shape;

    // `primitives` is read during construction only and is not retained.
    explicit ShapeBvh(std::span<const Primitive* const> primitives, const BvhBuildSettings& settings = {});

    [[nodiscard]] bool hit(const Ray& r, const Interval& ray_t, HitRecord& rec) const override;

    [[nodiscard]] Aabb bounding_box() const override;

    [[nodiscard]] const BvhTopology& topology() const noexcept { return topology_; }

private:
    BvhTopology topology_;
    std::vector<Shape> shapes_;              // Leaf order: all the intersection test reads
    std::vector<const Material*> materials_; // Leaf order, beside shapes_: read once, for the winner
};

using SphereBvh = ShapeBvh<Sphere>;
using QuadBvh = ShapeBvh<Quad>;

// Instantiated once, in shape_bvh.cpp: the traversal is compiled there, not in every includer.
extern template class ShapeBvh<Sphere>;
extern template class ShapeBvh<Quad>;

/// Builds the set in `arena`; with non-null `stats`, its counts and build time are folded in.
template <SetPrimitive Primitive>
[[nodiscard]] const ShapeBvh<Primitive>* make_shape_bvh(
    Arena<Hittable>& arena, std::type_identity_t<std::span<const Primitive* const>> primitives,
    BvhStats* stats = nullptr, const BvhBuildSettings& settings = {}) {
    // Timed around the arena call: bounds, build and gather are all part of what the scene pays.
    const auto start = std::chrono::steady_clock::now();
    const ShapeBvh<Primitive>* tree = arena.create<ShapeBvh<Primitive>>(primitives, settings);
    const auto end = std::chrono::steady_clock::now();

    // A scene builds one tree per mesh, group and set; these are its totals.
    if (stats != nullptr) stats->record(tree->topology(), std::chrono::duration_cast<std::chrono::nanoseconds>(end - start));
    return tree;
}

} // namespace pt
