#include "pt/geometry/shape_bvh.hpp"
#include "pt/core/hit_record.hpp"
#include "pt/geometry/bvh.hpp"
#include "pt/geometry/quad.hpp"
#include "pt/geometry/sphere.hpp"
#include "pt/math/aabb.hpp"
#include "pt/math/interval.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/scalar.hpp"
#include "pt/util/stats.hpp"
#include <cassert>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace pt {

template <SetPrimitive Primitive>
ShapeBvh<Primitive>::ShapeBvh(std::span<const Primitive* const> primitives, const BvhBuildSettings& settings) {
    assert(primitives.size() <= std::numeric_limits<std::uint32_t>::max());

    // The bounds the objects themselves report, in their order: the tree
    // is the one a generic Bvh would build over them.
    std::vector<Aabb> bounds;
    bounds.reserve(primitives.size());
    for (const auto* p : primitives) {
        bounds.push_back(Traits::bounds(p->shape()));
    }

    BvhBuildResult result = build_bvh_topology(bounds, Traits::kind, settings);
    topology_ = std::move(result.topology);

    // Gathered in leaf order. The order itself is not kept: nothing reads a slot back to its object.
    shapes_.reserve(primitives.size());
    materials_.reserve(primitives.size());
    for (const auto id : result.primitive_order) {
        shapes_.push_back(primitives[id]->shape());
        materials_.push_back(primitives[id]->material());
    }
}

template <SetPrimitive Primitive>
bool ShapeBvh<Primitive>::hit(const Ray& r, const Interval& ray_t, HitRecord& rec) const {
    std::uint32_t best_slot{};
    typename Traits::Hit best{};

    // Leaves run the geometric test only and remember the winner. A leaf searches only the interval traversal
    // hands it, which already ends at the nearest hit so far, so the last winner kept is the nearest overall.
    const bool found = bvh_closest_hit(
        topology_, r, ray_t, [this, &r, &best_slot, &best](const BvhNode& leaf, const Interval& open) -> std::optional<Float> {
            Float closest = open.max;
            bool leaf_hit{false};

            const std::uint32_t end = leaf.offset + static_cast<std::uint32_t>(leaf.count);
            for (std::uint32_t slot = leaf.offset; slot < end; ++slot) {
                count_leaf_test();

                typename Traits::Hit candidate{};
                if (Traits::intersect(r, Interval(open.min, closest), shapes_[slot], candidate)) {
                    closest = Traits::distance(candidate);
                    best_slot = slot;
                    best = candidate;
                    leaf_hit = true;
                }
            }

            return leaf_hit ? std::optional<Float>(closest) : std::nullopt;
        });

    if (!found) return false;

    // Written once, for the hit that survived: a miss leaves the caller's record untouched.
    Traits::fill(r, shapes_[best_slot], best, materials_[best_slot], rec);
    return true;
}

template <SetPrimitive Primitive>
Aabb ShapeBvh<Primitive>::bounding_box() const { return topology_.nodes.empty() ? Aabb() : topology_.nodes[0].bbox; }

// The two kinds the engine has; the header declares them extern.
template class ShapeBvh<Sphere>;
template class ShapeBvh<Quad>;

} // namespace pt
