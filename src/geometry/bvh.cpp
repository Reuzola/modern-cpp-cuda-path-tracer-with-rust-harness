#include "pt/geometry/bvh.hpp"
#include "pt/core/hit_record.hpp"
#include "pt/core/hittable.hpp"
#include "pt/core/hittable_list.hpp"
#include "pt/math/aabb.hpp"
#include "pt/math/interval.hpp"
#include "pt/math/scalar.hpp"
#include "pt/util/arena.hpp"
#include "pt/util/stats.hpp"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <span>
#include <vector>

namespace pt {

namespace {

// Leaf threshold, chosen by measurement; becomes a build parameter in the SAH work.
constexpr std::size_t max_leaf_size = 4;

// Sorts by the lower bound of the box on the split axis, matching the pointer-based build.
[[nodiscard]] bool box_compare(const Hittable* a, const Hittable* b, int axis_index) {
    return a->bounding_box().axis_interval(axis_index).min < b->bounding_box().axis_interval(axis_index).min;
}

} // namespace

Bvh::Bvh(std::span<const Hittable* const> objects) : primitives_(objects.begin(), objects.end()) {
    if (primitives_.empty()) return;
    assert(primitives_.size() <= std::numeric_limits<std::uint32_t>::max());

    // A binary tree with one primitive per leaf has exactly 2N-1 nodes; one allocation covers the build.
    // Upper bound: with larger leaves the tree holds fewer nodes, so this over-reserves a little.
    nodes_.reserve(2 * primitives_.size() - 1);
    build(0, primitives_.size(), 0);
}

bool Bvh::hit(const Ray& r, const Interval& ray_t, HitRecord& rec, Sampler& sampler) const {
    if (nodes_.empty()) return false;
    return hit_node(0, r, ray_t, rec, sampler);
}

Aabb Bvh::bounding_box() const { return nodes_.empty() ? Aabb() : nodes_[0].bbox; }

std::uint32_t Bvh::build(std::size_t first, std::size_t count, int depth) {
    const std::uint32_t index = static_cast<std::uint32_t>(nodes_.size());
    nodes_.emplace_back();
    max_depth_ = std::max(max_depth_, depth);

    Aabb bbox;
    for (std::size_t i = first; i < first + count; ++i) {
        bbox = Aabb(bbox, primitives_[i]->bounding_box());
    }

    if (count <= max_leaf_size) {
        nodes_[index].bbox = bbox;
        nodes_[index].offset = static_cast<std::uint32_t>(first);
        nodes_[index].count = static_cast<std::uint16_t>(count);
        ++leaf_count_;
        return index;
    }

    const int axis = bbox.longest_axis();
    const auto begin = std::next(primitives_.begin(), static_cast<std::ptrdiff_t>(first));
    std::sort(begin, std::next(begin, static_cast<std::ptrdiff_t>(count)),
              [axis](const Hittable* a, const Hittable* b) { return box_compare(a, b, axis); });
    const std::size_t mid = count / 2;

    build(first, mid, depth + 1); // lands at index + 1
    const std::uint32_t right = build(first + mid, count - mid, depth + 1);

    // Written through the index, not a reference: the recursive calls above may have reallocated nodes_.
    nodes_[index].bbox = bbox;
    nodes_[index].offset = right;
    return index;
}

bool Bvh::hit_node(std::uint32_t index, const Ray& r, const Interval& ray_t, HitRecord& rec, Sampler& sampler) const {
    const BvhNode& node = nodes_[index];
    count_node_test();

    if (!node.bbox.hit(r, ray_t)) return false;

    if (node.is_leaf()) {
        bool is_hit = false;
        Float closest = ray_t.max;
        for (std::uint32_t i = node.offset; i < node.offset + node.count; ++i) {
            count_leaf_test();

            if (primitives_[i]->hit(r, Interval(ray_t.min, closest), rec, sampler)) {
                is_hit = true;
                closest = rec.t;
            }
        }
        return is_hit;
    }

    // Left child is implicit at index + 1; the right subtree is only entered inside the narrowed interval.
    const bool hit_left = hit_node(index + 1, r, ray_t, rec, sampler);
    const bool hit_right = hit_node(node.offset, r, Interval(ray_t.min, hit_left ? rec.t : ray_t.max), rec, sampler);
    return hit_left || hit_right;
}

const Bvh* make_bvh(Arena<Hittable>& arena, const HittableList& list, BvhStats* stats) {
    const auto start = std::chrono::steady_clock::now();
    const Bvh* root = arena.create<Bvh>(list.objects());
    const auto end = std::chrono::steady_clock::now();

    if (stats != nullptr) {
        stats->build_time += std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
        ++stats->bvh_count;
        stats->node_count += root->node_count();
        stats->leaf_count += root->leaf_count();
        stats->max_depth = std::max(stats->max_depth, root->max_depth());
    }

    return root;
}

} // namespace pt
