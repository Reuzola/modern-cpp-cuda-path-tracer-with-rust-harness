#pragma once
#include "pt/core/hit_record.hpp"
#include "pt/geometry/bottom_level_bvh.hpp"
#include "pt/geometry/bvh.hpp"
#include "pt/geometry/instance.hpp"
#include "pt/geometry/level.hpp"
#include "pt/geometry/primitive_store.hpp"
#include "pt/math/aabb.hpp"
#include "pt/math/interval.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/scalar.hpp"
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

namespace pt {

// What a closest-hit query found, before anything is derived from it: the winning primitive's report and the
// instance it was met through. Enough to fill the record later; holds nothing the fill would compute.
struct SurfaceHit {
    // A winner that is one of the top level's own primitives, met through no instance.
    static constexpr std::uint32_t no_instance = std::numeric_limits<std::uint32_t>::max();

    PrimitiveHit primitive;
    std::uint32_t instance = no_instance; // Into the top level's instances, in leaf order

    [[nodiscard]] constexpr Float t() const noexcept { return primitive.t; }
};

// The tree a query starts from: world-space primitives and instances side by side, so geometry that needs no
// transform shares one tree with the placements around it. An instance leaf moves the ray into the space of
// the bottom-level tree it names and walks that tree; nothing below it can hold another instance, so a ray
// crosses at most one boundary. A query reports the winner across both levels and fills nothing; a caller
// that still wants the winner fills its record from that report, once.
class TopLevelBvh {
public:
    // A world with nothing in it: every query misses.
    TopLevelBvh() = default;

    // Every instance among `items` names a tree of `bottom_levels` by index; the tree takes them over.
    TopLevelBvh(std::span<const LevelItem> items, std::vector<BottomLevelBvh> bottom_levels, const BvhBuildSettings& settings = {});

    // The closest hit inside ray_t, as the leaves reported it, written into `out`; false on a miss, which leaves
    // `out` untouched. Fills no record: the caller may find that something else - a medium in front of the
    // surface - takes its place. An out-parameter rather than an optional, as the kernels below it use: the leaf
    // scans write the winner straight into the caller's report, with no local copy to clear and hand back.
    [[nodiscard]] bool intersect(const Ray& r, const Interval& ray_t, SurfaceHit& out) const noexcept;

    // Writes the record of a hit intersect() returned for the same ray. An instanced winner's ray is moved into
    // its space again here rather than kept from the query: the same transform of the same ray, bit for bit.
    void fill(const SurfaceHit& hit, const Ray& r, HitRecord& rec) const noexcept;

    // Closest hit inside ray_t with its record, or false. A miss leaves the record untouched.
    [[nodiscard]] bool hit(const Ray& r, const Interval& ray_t, HitRecord& rec) const noexcept {
        SurfaceHit found;
        if (!intersect(r, ray_t, found)) return false;
        fill(found, r, rec);
        return true;
    }

    // Root bounds, or an empty box when the world is empty.
    [[nodiscard]] Aabb bounding_box() const noexcept;

    [[nodiscard]] const BvhTopology& topology() const noexcept { return topology_; }
    [[nodiscard]] std::size_t instance_count() const noexcept { return instances_.size(); }
    [[nodiscard]] std::span<const BottomLevelBvh> bottom_levels() const noexcept { return bottom_levels_; }

private:
    BvhTopology topology_;
    PrimitiveStore store_;                      // This tree's own primitives
    std::vector<Instance> instances_;           // Leaf order
    std::vector<BottomLevelBvh> bottom_levels_; // What instances_ name by index
};

// In the header like the bottom level's fill: the dispatch is one branch, and a call around it costs more than it does.
inline void TopLevelBvh::fill(const SurfaceHit& hit, const Ray& r, HitRecord& rec) const noexcept {
    if (hit.instance == SurfaceHit::no_instance) {
        store_.fill(hit.primitive, r, rec);
        return;
    }

    const Instance& instance = instances_[hit.instance];
    bottom_levels_[instance.bvh()].fill(hit.primitive, instance.to_local(r), rec);
    instance.to_world(rec);
}

} // namespace pt
