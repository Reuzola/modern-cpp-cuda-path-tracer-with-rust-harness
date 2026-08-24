#pragma once
#include "pt/core/hit_record.hpp"
#include "pt/core/hittable.hpp"
#include "pt/math/aabb.hpp"
#include "pt/math/interval.hpp"
#include "pt/math/scalar.hpp"
#include "pt/util/arena.hpp"
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace pt {

class Ray;
class HittableList;

// Binned SAH build parameters (cost ratio determines tree shape).
// Integer fields are clamped: bin_count in [2, 32], max_leaf_size in [1, 65535].
struct BvhBuildSettings {
    Float traversal_cost{1.0_f};
    Float intersection_cost{8.0_f};
    int bin_count{12};
    int max_leaf_size{4};
};

// Scene-wide BVH statistics accumulated across all meshes, groups, and root trees.
struct BvhStats {
    std::size_t bvh_count{};               // Trees folded in so far
    std::size_t node_count{};              // Interior and leaf nodes, summed over all trees
    std::size_t leaf_count{};              // Leaf nodes, summed over all trees
    int max_depth{};                       // Deepest node of any tree (each tree's root is depth 0)
    std::chrono::nanoseconds build_time{}; // Summed over all trees; integer ns, so lossless
};

// One node of the flat BVH. Nodes are laid out depth first, which places the left
// child directly after its parent, so only the right child needs a stored index.
struct BvhNode {
    Aabb bbox;
    std::uint32_t offset{}; // Right child index when interior, first primitive index when leaf
    std::uint16_t count{};  // Primitive count; 0 is what marks the node interior

    [[nodiscard]] constexpr bool is_leaf() const noexcept { return count > 0; }
};

// Layout guard: bounds + index data (32B float / 56B double) to preserve hot loop cache efficiency.
static_assert(sizeof(BvhNode) == 6 * sizeof(Float) + 8);

// Read-only BVH built via binned SAH. Non-owning over primitives (permuted for contiguous leaves).
// Implements Hittable to support nesting (e.g., mesh and group subtrees).
class Bvh final : public Hittable {
public:
    // `objects` is read during construction only and is not retained.
    explicit Bvh(std::span<const Hittable* const> objects, const BvhBuildSettings& settings = {});

    // Closest hit inside ray_t, or false when the ray misses the tree entirely.
    [[nodiscard]] bool hit(const Ray& r, const Interval& ray_t, HitRecord& rec) const override;

    // Root bounds, or an empty box when the tree holds no primitives.
    [[nodiscard]] Aabb bounding_box() const override;

    // The three counters below exist for reporting; traversal never reads them.
    [[nodiscard]] std::size_t node_count() const noexcept { return nodes_.size(); }

    [[nodiscard]] std::size_t leaf_count() const noexcept { return leaf_count_; }

    [[nodiscard]] int max_depth() const noexcept { return max_depth_; }

private:
    std::vector<BvhNode> nodes_;              // Depth first; nodes_[0] is the root when non-empty
    std::vector<const Hittable*> primitives_; // Permuted by the build; leaves index into this
    std::size_t leaf_count_{};
    int max_depth_{};
};

/// Builds a BVH over `list` and stores it in `arena`.
/// The tree is owned by the arena and stays valid as long as the arena does. `list` itself is not
/// retained, but the primitives it points at must outlive the arena.
/// @param stats If non-null, this tree's counts and build time are folded into it.
[[nodiscard]] const Bvh* make_bvh(Arena<Hittable>& arena, const HittableList& list,
                                  BvhStats* stats = nullptr, const BvhBuildSettings& settings = {});

} // namespace pt
