#include "pt/geometry/bvh.hpp"
#include "pt/core/hit_record.hpp"
#include "pt/core/hittable.hpp"
#include "pt/core/hittable_list.hpp"
#include "pt/math/aabb.hpp"
#include "pt/math/interval.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
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
#include <utility>
#include <vector>

namespace pt {

namespace {

// Leaf threshold, chosen by measurement; becomes a build parameter in the SAH work.
constexpr std::size_t max_leaf_size = 4;

// Per-primitive build data, computed once up front. Hittable::bounding_box() is virtual and
// MeshTriangle recomputes it from vertices on every call, so the build must not ask twice.
struct PrimitiveInfo {
    const Hittable* primitive = nullptr;
    Aabb bbox;
    Point3 centroid;
};

// What the builder hands back to Bvh. Build-time scratch state stays inside the builder.
struct BvhBuildResult {
    std::vector<BvhNode> nodes;
    std::vector<const Hittable*> primitives;
    std::size_t leaf_count{};
    int max_depth{};
};

// Top-down BVH builder. Owns the scratch state a build needs and nothing else: the finished
// Bvh carries no build-time data. A separate type also keeps that state out of the public header.
class BvhBuilder {
public:
    explicit BvhBuilder(std::span<const Hittable* const> objects) {
        info_.reserve(objects.size());
        for (const Hittable* obj : objects) {
            const Aabb box = obj->bounding_box();
            info_.push_back(PrimitiveInfo{.primitive = obj, .bbox = box, .centroid = box.centroid()});
        }

        // A binary tree with one primitive per leaf has exactly 2N-1 nodes; one allocation covers the build.
        // Upper bound: with larger leaves the tree holds fewer nodes, so this over-reserves a little.
        if (!info_.empty()) nodes_.reserve(2 * info_.size() - 1);
    }

    // Consumes the builder to construct and return the BVH.
    [[nodiscard]] BvhBuildResult build() && {
        if (info_.empty()) return {};
        assert(info_.size() <= std::numeric_limits<std::uint32_t>::max());

        build_recursive(0, info_.size(), 0);

        std::vector<const Hittable*> primitives;
        primitives.reserve(info_.size());
        for (const PrimitiveInfo& entry : info_) {
            primitives.push_back(entry.primitive);
        }

        return BvhBuildResult{
            .nodes = std::move(nodes_), .primitives = std::move(primitives), .leaf_count = leaf_count_, .max_depth = max_depth_};
    }

private:
    std::vector<PrimitiveInfo> info_;
    std::vector<BvhNode> nodes_;
    std::size_t leaf_count_{};
    int max_depth_{};

    std::uint32_t build_recursive(std::size_t first, std::size_t count, int depth) {
        const std::uint32_t index = static_cast<std::uint32_t>(nodes_.size());
        nodes_.emplace_back();
        max_depth_ = std::max(max_depth_, depth);

        Aabb bbox;
        for (std::size_t i = first; i < first + count; ++i) {
            bbox = Aabb(bbox, info_[i].bbox);
        }

        if (count <= max_leaf_size) {
            nodes_[index].bbox = bbox;
            nodes_[index].offset = static_cast<std::uint32_t>(first);
            nodes_[index].count = static_cast<std::uint16_t>(count);
            ++leaf_count_;
            return index;
        }

        const int axis = bbox.longest_axis();
        const auto begin = std::next(info_.begin(), static_cast<std::ptrdiff_t>(first));
        std::sort(begin, std::next(begin, static_cast<std::ptrdiff_t>(count)),
                  [axis](const PrimitiveInfo& a, const PrimitiveInfo& b) {
                      return a.bbox.axis_interval(axis).min < b.bbox.axis_interval(axis).min;
                  });
        const std::size_t mid = count / 2;

        build_recursive(first, mid, depth + 1); // lands at index + 1
        const std::uint32_t right = build_recursive(first + mid, count - mid, depth + 1);

        // Written through the index, not a reference: the recursive calls above may have reallocated nodes_.
        nodes_[index].bbox = bbox;
        nodes_[index].offset = right;
        return index;
    }
};

} // namespace

Bvh::Bvh(std::span<const Hittable* const> objects) {
    // Assigned in the body rather than the init list: all four members come out of a single build pass.
    BvhBuildResult result = BvhBuilder(objects).build();

    nodes_ = std::move(result.nodes);
    primitives_ = std::move(result.primitives);
    leaf_count_ = result.leaf_count;
    max_depth_ = result.max_depth;
}

bool Bvh::hit(const Ray& r, const Interval& ray_t, HitRecord& rec, Sampler& sampler) const {
    if (nodes_.empty()) return false;
    return hit_node(0, r, ray_t, rec, sampler);
}

Aabb Bvh::bounding_box() const { return nodes_.empty() ? Aabb() : nodes_[0].bbox; }

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
