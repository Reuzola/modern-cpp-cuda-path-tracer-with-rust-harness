#pragma once
#include "pt/core/hit_record.hpp"
#include "pt/geometry/primitive_kind.hpp"
#include "pt/geometry/quad.hpp"
#include "pt/geometry/sphere.hpp"
#include "pt/geometry/triangle.hpp"
#include "pt/math/interval.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/scalar.hpp"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace pt {

class Material;
class Mesh;
struct BvhNode;

// One candidate of one kind: its slot in that kind's arrays, and what that kind's test returned.
template <typename Hit>
struct SlotHit {
    std::uint32_t slot{};
    Hit hit{};
};

// The nearest candidate a query has met so far, one entry per kind rather than one shared variant: each
// kernel writes only its own. A leaf reports only a candidate nearer than every earlier one, so `kind`,
// set by the latest report, names the winner.
struct PrimitiveHit {
    SlotHit<Float> sphere;
    SlotHit<QuadHit> quad;
    SlotHit<TriangleHit> triangle;
    PrimitiveKind kind{};
};

// The primitives of one tree, each kind in its own arrays and laid out in the tree's leaf order, so a leaf
// names a contiguous run of one kind. Spheres and quads are copied in with their materials; a triangle's
// corners are copied and its attributes read back from its mesh, for the candidate that wins only.
// Owns no tree: whoever holds the topology hands it the leaves of the kinds it holds.
class PrimitiveStore {
public:
    // Room for the given counts, so the appends below never reallocate.
    void reserve(std::size_t spheres, std::size_t quads, std::size_t triangles);

    // Each appends one primitive at the end of its kind's arrays and returns the slot it took there. Called in
    // leaf order, which is what makes a leaf's run contiguous. A triangle's mesh must outlive the store, and
    // for now one store holds the triangles of a single mesh.
    [[nodiscard]] std::uint32_t add_sphere(const Sphere& sphere);
    [[nodiscard]] std::uint32_t add_quad(const Quad& quad);
    [[nodiscard]] std::uint32_t add_triangle(const Mesh& mesh, std::uint32_t triangle_index);

    // Tests one leaf of a kind this store holds inside `open`, narrowing as it goes. A candidate that narrows is
    // written into `hit`, and a leaf that found one sets `hit.kind` to its own. Returns the nearest distance, or nothing.
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

    // Corners in leaf order for the test; the mesh and each slot's triangle index for the attributes of the one that wins.
    struct TriangleArrays {
        std::vector<TriangleCorners> corners;
        std::vector<std::uint32_t> ids; // Leaf slot to the mesh's own triangle index
        const Mesh* mesh = nullptr;
    };

    ShapeArrays<SphereShape> spheres_;
    ShapeArrays<QuadShape> quads_;
    TriangleArrays triangles_;

    void fill_triangle(const Ray& r, const SlotHit<TriangleHit>& winner, HitRecord& rec) const noexcept;
};

} // namespace pt
