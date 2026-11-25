#include "pt/geometry/top_level_bvh.hpp"
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

bool TopLevelBvh::intersect(const Ray& r, const Interval& ray_t, SurfaceHit& out) const noexcept {
    // The caller's report is the one candidate state for both levels. Every report is nearer than all before it,
    // wherever it came from, so the kind of the latest report and the instance it came through name the winner
    // together. Written only by reports, which is what leaves `out` untouched on a miss.
    const std::optional<Float> found = bvh_closest_hit(
        topology_, r, ray_t, [&](const BvhNode& leaf, const Interval& open) -> std::optional<Float> {
            if (is_primitive_kind(leaf.kind)) {
                const std::optional<Float> t = store_.closest_in_leaf(leaf, r, open, out.primitive);
                if (t) out.instance = SurfaceHit::no_instance;
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
                const std::optional<Float> t = bottom_levels_[instance.bvh()].closest(local, Interval(open.min, closest), out.primitive);
                if (t) {
                    closest = *t;
                    out.instance = slot;
                    leaf_hit = true;
                }
            }

            return leaf_hit ? std::optional<Float>(closest) : std::nullopt;
        });

    return found.has_value();
}

Aabb TopLevelBvh::bounding_box() const noexcept { return topology_.nodes.empty() ? Aabb() : topology_.nodes[0].bbox; }

} // namespace pt
