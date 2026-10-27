#include "pt/geometry/top_level_bvh.hpp"
#include "pt/core/hit_record.hpp"
#include "pt/geometry/bottom_level_bvh.hpp"
#include "pt/geometry/bvh.hpp"
#include "pt/geometry/bvh_node.hpp"
#include "pt/geometry/instance.hpp"
#include "pt/geometry/level.hpp"
#include "pt/geometry/primitive_kind.hpp"
#include "pt/geometry/primitive_store.hpp"
#include "pt/math/aabb.hpp"
#include "pt/math/interval.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/scalar.hpp"
#include "pt/util/stats.hpp"
#include <cstdint>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace pt {

TopLevelBvh::TopLevelBvh(std::span<const LevelItem> items, std::vector<BottomLevelBvh> bottom_levels, const BvhBuildSettings& settings)
    : bottom_levels_(std::move(bottom_levels)) {
    std::vector<Aabb> bounds;
    bounds.reserve(bottom_levels_.size());
    for (const BottomLevelBvh& level : bottom_levels_) {
        bounds.push_back(level.bounding_box());
    }

    LevelBuild build = build_level(items, bounds, settings);
    topology_ = std::move(build.topology);
    store_ = std::move(build.store);
    instances_ = std::move(build.instances);
}

bool TopLevelBvh::hit(const Ray& r, const Interval& ray_t, HitRecord& rec) const {
    // One candidate state for both levels. Every report is nearer than all before it, wherever it came from,
    // so the kind of the latest report and the instance it came through name the winner together.
    PrimitiveHit hit;
    std::uint32_t winner = no_instance;
    Ray winner_ray = r; // The ray the winner was found with: the fill must see that ray, not a recomputed one.

    const std::optional<Float> found = bvh_closest_hit(
        topology_, r, ray_t, [&](const BvhNode& leaf, const Interval& open) -> std::optional<Float> {
            if (is_primitive_kind(leaf.kind)) {
                const std::optional<Float> t = store_.closest_in_leaf(leaf, r, open, hit);
                if (t) winner = no_instance;
                return t;
            }

            // An instance run, narrowing as it goes like any other leaf.
            Float closest = open.max;
            bool leaf_hit{false};

            const std::uint32_t end = leaf.offset + static_cast<std::uint32_t>(leaf.count);
            for (std::uint32_t slot = leaf.offset; slot < end; ++slot) {
                count_leaf_test();

                const Instance& instance = instances_[slot];
                const Ray local = instance.to_local(r);
                const std::optional<Float> t = bottom_levels_[instance.bvh()].closest(local, Interval(open.min, closest), hit);
                if (t) {
                    closest = *t;
                    winner = slot;
                    winner_ray = local;
                    leaf_hit = true;
                }
            }

            return leaf_hit ? std::optional<Float>(closest) : std::nullopt;
        });

    if (!found) return false;

    // Written once, for the hit that survived: a miss leaves the caller's record untouched.
    if (winner == no_instance) {
        store_.fill(hit, r, rec);
        return true;
    }

    const Instance& instance = instances_[winner];
    bottom_levels_[instance.bvh()].fill(hit, winner_ray, rec);
    instance.to_world(rec);
    return true;
}

Aabb TopLevelBvh::bounding_box() const noexcept { return topology_.nodes.empty() ? Aabb() : topology_.nodes[0].bbox; }

} // namespace pt
