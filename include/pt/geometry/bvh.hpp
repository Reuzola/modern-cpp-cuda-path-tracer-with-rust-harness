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
class Sampler;
class HittableList;

struct BvhStats {
    std::size_t bvh_count{};
    std::size_t node_count{};
    std::size_t leaf_count{};              // Leaf nodes; each primitive is referenced exactly once
    int max_depth{};                       // Depth of the deepest node (root = 0)
    std::chrono::nanoseconds build_time{}; // Total time in ns; lossless integer sum across trees
};

// Flat BVH node. In depth-first layout the left child always sits at index + 1,
// so only the right child needs an explicit index.
struct BvhNode {
    Aabb bbox;
    std::uint32_t offset{}; // Right child index when interior, first primitive index when leaf
    std::uint16_t count{};  // 0 marks an interior node; leaf sizes stay far below the 16-bit range

    [[nodiscard]] constexpr bool is_leaf() const noexcept { return count > 0; }
};

// Layout guard: bounds plus one aligned word of index data (32 bytes with float, 56 with double).
static_assert(sizeof(BvhNode) == 6 * sizeof(Float) + 8);

class Bvh final : public Hittable {
public:
    explicit Bvh(std::span<const Hittable* const> objects);

    [[nodiscard]] bool hit(const Ray& r, const Interval& ray_t, HitRecord& rec, Sampler& sampler) const override;

    [[nodiscard]] Aabb bounding_box() const override;

    [[nodiscard]] std::size_t node_count() const noexcept { return nodes_.size(); }

    [[nodiscard]] std::size_t leaf_count() const noexcept { return leaf_count_; }

    [[nodiscard]] int max_depth() const noexcept { return max_depth_; }

private:
    std::vector<BvhNode> nodes_;
    std::vector<const Hittable*> primitives_;
    std::size_t leaf_count_{};
    int max_depth_{};

    [[nodiscard]] bool hit_node(std::uint32_t index, const Ray& r, const Interval& ray_t, HitRecord& rec, Sampler& sampler) const;
};

[[nodiscard]] const Bvh* make_bvh(Arena<Hittable>& arena, const HittableList& list, BvhStats* stats = nullptr);

} // namespace pt
