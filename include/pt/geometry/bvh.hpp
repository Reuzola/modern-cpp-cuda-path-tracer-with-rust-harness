#pragma once
#include "pt/core/hit_record.hpp"
#include "pt/core/hittable.hpp"
#include "pt/core/hittable_list.hpp"
#include "pt/math/aabb.hpp"
#include "pt/math/interval.hpp"
#include "pt/util/arena.hpp"
#include <chrono>
#include <cstddef>
#include <span>

namespace pt {

class Ray;

class Sampler;

struct BvhStats {
    std::size_t bvh_count{};
    std::size_t node_count{};
    std::size_t leaf_count{}; // Link connections, not unique primitives (single item links twice)
    int max_depth{}; // Depth of the deepest node (root = 0)
    std::chrono::nanoseconds build_time{}; // Total time in ns; lossless integer sum across trees
};

class BvhNode final : public Hittable {
public:
    BvhNode(Arena<Hittable>& arena, const HittableList& list);

    BvhNode(Arena<Hittable>& arena, std::span<const Hittable*> objects);

    [[nodiscard]] bool hit(const Ray& r, const Interval& ray_t, HitRecord& rec, Sampler& sampler) const override;

    [[nodiscard]] Aabb bounding_box() const override;

private:
    const Hittable* left_ = nullptr;
    const Hittable* right_ = nullptr;
    bool left_is_node_{false};
    bool right_is_node_{false};
    Aabb bbox_;

    void build(Arena<Hittable>& arena, std::span<const Hittable*> objects);

    void accumulate_stats(BvhStats& stats, int depth) const;

    static bool box_compare(const Hittable* a, const Hittable* b, int axis_index);

    static bool box_x_compare(const Hittable* a, const Hittable* b);
    static bool box_y_compare(const Hittable* a, const Hittable* b);
    static bool box_z_compare(const Hittable* a, const Hittable* b);

    friend const BvhNode* make_bvh(Arena<Hittable>& arena, const HittableList& list, BvhStats* stats);
};

[[nodiscard]] const BvhNode* make_bvh(Arena<Hittable>& arena, const HittableList& list, BvhStats* stats = nullptr);

} // namespace pt
