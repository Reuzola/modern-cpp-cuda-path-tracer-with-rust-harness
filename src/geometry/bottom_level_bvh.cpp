#include "pt/geometry/bottom_level_bvh.hpp"
#include "pt/geometry/bvh.hpp"
#include "pt/geometry/bvh_node.hpp"
#include "pt/geometry/level.hpp"
#include "pt/geometry/primitive_store.hpp"
#include "pt/math/aabb.hpp"
#include "pt/math/interval.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/scalar.hpp"
#include <cassert>
#include <optional>
#include <span>
#include <utility>

namespace pt {

BottomLevelBvh::BottomLevelBvh(std::span<const LevelItem> items, const BvhBuildSettings& settings) {
    LevelBuild build = build_level(items, {}, settings);
    assert(build.instances.empty());

    topology_ = std::move(build.topology);
    store_ = std::move(build.store);
}

std::optional<Float> BottomLevelBvh::closest(const Ray& r, const Interval& ray_t, PrimitiveHit& hit) const noexcept {
    return bvh_closest_hit(topology_, r, ray_t, [this, &r, &hit](const BvhNode& leaf, const Interval& open) {
        return store_.closest_in_leaf(leaf, r, open, hit);
    });
}

Aabb BottomLevelBvh::bounding_box() const noexcept { return topology_.nodes.empty() ? Aabb() : topology_.nodes[0].bbox; }

} // namespace pt
