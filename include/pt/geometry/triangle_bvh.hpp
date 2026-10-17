#pragma once
#include "pt/core/hit_record.hpp"
#include "pt/core/hittable.hpp"
#include "pt/geometry/bvh.hpp"
#include "pt/geometry/triangle.hpp"
#include "pt/math/aabb.hpp"
#include "pt/math/interval.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/vec3.hpp"
#include "pt/util/arena.hpp"
#include <cstdint>
#include <vector>

namespace pt {

class Mesh;

// A mesh's triangles under a BVH of their own. The corners are copied into leaf order, so a leaf's test
// reads one contiguous run of memory and no per-triangle object exists. The mesh keeps its indexed buffers
// for the attributes, which are read only for the hit that wins. Non-owning: the mesh must outlive the tree.
class TriangleBvh final : public Hittable {
public:
    explicit TriangleBvh(const Mesh& mesh, const BvhBuildSettings& settings = {});

    [[nodiscard]] bool hit(const Ray& r, const Interval& ray_t, HitRecord& rec) const override;

    [[nodiscard]] Aabb bounding_box() const override;

    [[nodiscard]] const BvhTopology& topology() const noexcept { return topology_; }

private:
    // What the intersection test reads, and all it reads. The layout is this class's own
    // business, so a packed form can replace it without touching anything outside.
    struct Corners {
        Point3 v0;
        Point3 v1;
        Point3 v2;
    };

    const Mesh* mesh_ = nullptr; // Attributes and material, read once per closest hit and never by the test
    BvhTopology topology_;
    std::vector<Corners> triangles_;          // Leaf order: a leaf's (offset, count) is a contiguous run of these
    std::vector<std::uint32_t> triangle_ids_; // Leaf slot to the mesh's own triangle index

    void fill_record(const Ray& r, std::uint32_t slot, const TriangleHit& hit, HitRecord& rec) const noexcept;
};

/// Builds the tree for `mesh` in `arena`; with non-null `stats`, its counts and build time are folded in.
[[nodiscard]] const TriangleBvh* make_triangle_bvh(Arena<Hittable>& arena, const Mesh& mesh,
                                                   BvhStats* stats = nullptr, const BvhBuildSettings& settings = {});

} // namespace pt
