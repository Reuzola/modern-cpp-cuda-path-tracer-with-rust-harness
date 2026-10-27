#pragma once
#include "pt/core/hit_record.hpp"
#include "pt/geometry/bvh.hpp"
#include "pt/geometry/level.hpp"
#include "pt/geometry/primitive_store.hpp"
#include "pt/math/aabb.hpp"
#include "pt/math/interval.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/scalar.hpp"
#include <optional>
#include <span>

namespace pt {

// A tree over one level's primitives in their own coordinate space, placed in the world only through the
// instances that name it. Holds no instance itself, which is what keeps every ray to two levels at most.
class BottomLevelBvh {
public:
    // Every item but an instance. The objects behind them must outlive the tree.
    explicit BottomLevelBvh(std::span<const LevelItem> items, const BvhBuildSettings& settings = {});

    // The nearest candidate inside ray_t, written into `hit`; its distance, or nothing on a miss. Fills
    // no record: the caller decides which candidate across all its trees is the one worth filling.
    [[nodiscard]] std::optional<Float> closest(const Ray& r, const Interval& ray_t, PrimitiveHit& hit) const noexcept;

    // Writes the record of the candidate `hit` names; `r` must be the ray it was found with.
    void fill(const PrimitiveHit& hit, const Ray& r, HitRecord& rec) const noexcept { store_.fill(hit, r, rec); }

    // Root bounds, or an empty box when the tree holds no primitives.
    [[nodiscard]] Aabb bounding_box() const noexcept;

    [[nodiscard]] const BvhTopology& topology() const noexcept { return topology_; }

private:
    BvhTopology topology_;
    PrimitiveStore store_;
};

} // namespace pt
