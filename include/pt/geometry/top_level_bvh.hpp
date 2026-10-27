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
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

namespace pt {

// The tree a query starts from: world-space primitives and instances side by side, so geometry that needs no
// transform shares one tree with the placements around it. An instance leaf moves the ray into the space of
// the bottom-level tree it names and walks that tree; nothing below it can hold another instance, so a ray
// crosses at most one boundary. The record is filled once, for the candidate that won across both levels.
class TopLevelBvh {
public:
    // A world with nothing in it: every query misses.
    TopLevelBvh() = default;

    // Every instance among `items` names a tree of `bottom_levels` by index; the tree takes them over.
    TopLevelBvh(std::span<const LevelItem> items, std::vector<BottomLevelBvh> bottom_levels, const BvhBuildSettings& settings = {});

    // Closest hit inside ray_t, or false. A miss leaves the record untouched.
    [[nodiscard]] bool hit(const Ray& r, const Interval& ray_t, HitRecord& rec) const;

    // Root bounds, or an empty box when the world is empty.
    [[nodiscard]] Aabb bounding_box() const noexcept;

    [[nodiscard]] const BvhTopology& topology() const noexcept { return topology_; }
    [[nodiscard]] std::size_t instance_count() const noexcept { return instances_.size(); }
    [[nodiscard]] std::span<const BottomLevelBvh> bottom_levels() const noexcept { return bottom_levels_; }

private:
    // Which instance the winning candidate came from, or none when it is one of this tree's own primitives.
    static constexpr std::uint32_t no_instance = std::numeric_limits<std::uint32_t>::max();

    BvhTopology topology_;
    PrimitiveStore store_;                      // This tree's own primitives
    std::vector<Instance> instances_;           // Leaf order
    std::vector<BottomLevelBvh> bottom_levels_; // What instances_ name by index
};

} // namespace pt
