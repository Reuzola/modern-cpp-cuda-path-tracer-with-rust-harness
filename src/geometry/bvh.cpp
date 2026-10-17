#include "pt/geometry/bvh.hpp"
#include "pt/core/hit_record.hpp"
#include "pt/core/hittable.hpp"
#include "pt/core/hittable_list.hpp"
#include "pt/math/aabb.hpp"
#include "pt/math/constants.hpp"
#include "pt/math/interval.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include "pt/util/arena.hpp"
#include "pt/util/stats.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace pt {

namespace {

// Per-primitive build data, computed once from the caller's bounds. This array is what the build
// permutes; the primitive order is read off it at the end.
struct PrimitiveInfo {
    std::uint32_t index{};
    Aabb bbox;
    Point3 centroid; // Binned by this, bounded by bbox - a primitive is never split
};

// Widest axis of a centroid bounds, and its size.
struct AxisExtent {
    int axis{};
    Float extent{};
};

// Centroid bounds, tracked outside Aabb on purpose: Aabb's point ctor pads a
// degenerate axis relative to its own coordinate, which would hide coincident
// centroids exactly where the split has to detect them.
struct CentroidBounds {
    Point3 min{+infinity, +infinity, +infinity};
    Point3 max{-infinity, -infinity, -infinity};

    void extend(const Point3& c) noexcept {
        for (int k = 0; k < 3; ++k) {
            min[k] = std::fmin(min[k], c[k]);
            max[k] = std::fmax(max[k], c[k]);
        }
    }

    // Not Aabb::longest_axis(): that resolves ties to axis 2, this to the lowest index.
    // The tie-break decides the tree shape, so changing it invalidates every golden image.
    [[nodiscard]] AxisExtent widest_axis() const noexcept {
        const Float ex = max[0] - min[0];
        const Float ey = max[1] - min[1];
        const Float ez = max[2] - min[2];

        AxisExtent best{.axis = 0, .extent = ex};
        if (ey > best.extent) best = AxisExtent{.axis = 1, .extent = ey};
        if (ez > best.extent) best = AxisExtent{.axis = 2, .extent = ez};

        return best;
    }
};

// Maps a centroid onto the bin grid of one axis. Bins span the centroid bounds, not the node
// bounds, so every bin can actually receive primitives.
struct BinMapping {
    int axis{};
    Float min{};   // Centroid bounds minimum on `axis`
    Float scale{}; // bin_count / centroid extent on `axis`
    int bin_count{};

    // Clamped because the rightmost centroid maps exactly to bin_count.
    [[nodiscard]] int bin_of(const Point3& centroid) const noexcept {
        const int k = static_cast<int>((centroid[axis] - min) * scale);
        return std::clamp(k, 0, bin_count - 1);
    }
};

// One bin of the SAH sweep: how many primitives fell here and the union of their bounds.
struct Bin {
    Aabb bounds;
    int count{};
};

// Compile-time cap on bin_count. The bins live in a std::array so a node's split evaluation
// allocates nothing; bin_count is clamped to this range in the builder's constructor.
constexpr int max_bin_count = 32;

// Compile-time cap on max_leaf_size, imposed by the width of BvhNode::count. Clamped rather than
// asserted: a caller asking for huge leaves gets a slow tree, which is their problem, instead of a
// silent 16-bit wraparound in make_leaf(), which would be a corrupt tree.
constexpr int max_leaf_size_limit = std::numeric_limits<std::uint16_t>::max();

// Best split found for one node. Defaults to "none": cost infinity loses every comparison.
struct SplitCandidate {
    Float cost{infinity};
    int bin{-1}; // Last bin on the left side of the split; -1 when no usable candidate was found

    [[nodiscard]] constexpr bool is_valid() const noexcept { return bin >= 0; }
};

// Union of primitive bounds. Fixed iteration order, so the result is reproducible run to run.
[[nodiscard]] Aabb bounds_of(std::span<const PrimitiveInfo> prims) {
    Aabb bbox;
    for (const PrimitiveInfo& prim : prims) {
        bbox = Aabb(bbox, prim.bbox);
    }
    return bbox;
}

[[nodiscard]] CentroidBounds centroid_bounds_of(std::span<const PrimitiveInfo> prims) {
    CentroidBounds cb;
    for (const PrimitiveInfo& prim : prims) {
        cb.extend(prim.centroid);
    }
    return cb;
}

// Bins by centroid but accumulates full primitive bounds: the grid decides which side a primitive
// falls on, never how much space it occupies. Cheap to return by value: Bin is trivially copyable
// and the copy is elided in practice, though NRVO is permitted rather than guaranteed.
[[nodiscard]] std::array<Bin, max_bin_count> fill_bins(std::span<const PrimitiveInfo> prims, const BinMapping& mapping) {
    std::array<Bin, max_bin_count> bins{};
    for (const PrimitiveInfo& prim : prims) {
        Bin& bin = bins[static_cast<std::size_t>(mapping.bin_of(prim.centroid))];
        ++bin.count;
        bin.bounds = Aabb(bin.bounds, prim.bbox);
    }
    return bins;
}

// Reorders a node's primitives so the left side comes first, and returns the size of that side.
// stable_partition, not partition: equal-distance hits are resolved by primitive order, so an
// unstable reorder would silently change the image and break the golden set.
[[nodiscard]] std::size_t partition_by_bin(std::span<PrimitiveInfo> prims, const BinMapping& mapping, int split_bin) {
    const auto split_point = std::stable_partition(prims.begin(), prims.end(), [&](const PrimitiveInfo& entry) {
        return mapping.bin_of(entry.centroid) <= split_bin;
    });

    return static_cast<std::size_t>(std::distance(prims.begin(), split_point));
}

// Top-down BVH builder, one instance per tree. Recursion works in place on info_, narrowing to a contiguous [first, count)
// range per node; when it finishes, each leaf's primitives are adjacent, which is what lets a leaf be stored as (offset, count).
// A separate type keeps this state out of the finished topology and the public header.
class BvhBuilder {
public:
    BvhBuilder(std::span<const Aabb> primitive_bounds, const BvhBuildSettings& settings) : settings_(settings) {
        assert(primitive_bounds.size() <= std::numeric_limits<std::uint32_t>::max());
        settings_.bin_count = std::clamp(settings_.bin_count, 2, max_bin_count);
        settings_.max_leaf_size = std::clamp(settings_.max_leaf_size, 1, max_leaf_size_limit);
        settings_.depth_limit = std::clamp(settings_.depth_limit, 1, bvh_max_depth);

        for (std::uint32_t i = 0; i < primitive_bounds.size(); ++i) {
            const Aabb& box = primitive_bounds[i];
            info_.push_back(PrimitiveInfo{.index = i, .bbox = box, .centroid = box.centroid()});
        }

        // 2N-1 is the node count with one primitive per leaf, so it upper-bounds any larger-leaf tree.
        if (!info_.empty()) nodes_.reserve(2 * info_.size() - 1);
    }

    // Consumes the builder: info_ is left permuted and nodes_ moved from.
    [[nodiscard]] BvhBuildResult build() && {
        if (info_.empty()) return {};

        build_recursive(0, info_.size(), 0);

        std::vector<std::uint32_t> order;
        order.reserve(info_.size());
        for (const PrimitiveInfo& entry : info_) {
            order.push_back(entry.index);
        }

        return BvhBuildResult{
            .topology = {.nodes = std::move(nodes_), .leaf_count = leaf_count_, .max_depth = max_depth_},
            .primitive_order = std::move(order)};
    }

private:
    BvhBuildSettings settings_;       // Clamped copy, never the caller's original
    std::vector<PrimitiveInfo> info_; // Permuted in place by the build
    std::vector<BvhNode> nodes_;      // Appended depth first; a node's slot precedes its children
    std::size_t leaf_count_{};
    int max_depth_{};

    // View of one node's build entries. The build only ever works on contiguous ranges of info_.
    [[nodiscard]] std::span<PrimitiveInfo> range(std::size_t first, std::size_t count) noexcept {
        return std::span(info_).subspan(first, count);
    }

    // Sweeps the bin_count - 1 boundaries between bins and returns the cheapest one.
    // SAH cost of a split: traversal + (A_left * N_left + A_right * N_right) / A_node * intersection.
    // The division by A_node is a constant factor across candidates, but it is what puts the result on
    // the same scale as a leaf's count * intersection_cost, which build_recursive compares against.
    [[nodiscard]] SplitCandidate best_split(const std::array<Bin, max_bin_count>& bins, Float node_area) const {
        // Prefix scan results: entry j covers bins [0..j], the left side of boundary j.
        std::array<Float, max_bin_count> left_area{};
        std::array<int, max_bin_count> left_count{};

        // bin_count is clamped to >= 2, so both loops below run at least once.
        const std::size_t bins_used = static_cast<std::size_t>(settings_.bin_count);

        // Left to right: prefix area and count per boundary. The last bin is skipped - a boundary
        // past it would leave the right side empty.
        Aabb acc;
        int n{};
        for (std::size_t i = 0; i + 1 < bins_used; ++i) {
            acc = Aabb(acc, bins[i].bounds);
            n += bins[i].count;
            left_area[i] = acc.surface_area();
            left_count[i] = n;
        }

        // Right to left: i is the first bin on the right, so it pairs with prefix entry i - 1. The suffix is
        // accumulated in the same walk, which is why this direction cannot be merged into the loop above.
        acc = Aabb();
        n = 0;
        SplitCandidate best{};
        for (std::size_t i = bins_used - 1; i > 0; --i) {
            acc = Aabb(acc, bins[i].bounds);
            n += bins[i].count;

            // Empty sides are skipped, so a candidate that survives always splits the node in two.
            if (left_count[i - 1] != 0 && n != 0) {
                const Float left_contrib = left_area[i - 1] * static_cast<Float>(left_count[i - 1]);
                const Float right_contrib = acc.surface_area() * static_cast<Float>(n);
                const Float cost =
                    settings_.traversal_cost + (left_contrib + right_contrib) * settings_.intersection_cost / node_area;

                if (cost < best.cost) {
                    best.cost = cost;
                    best.bin = static_cast<int>(i) - 1; // Boundary i means bins [0..i-1] go left
                }
            }
        }

        return best;
    }

    // Both writers address nodes_ through the index rather than a reference: build_recursive may
    // have reallocated the vector between reserving a node's slot and filling it in.

    void make_leaf(std::uint32_t index, const Aabb& bbox, std::size_t first, std::size_t count) {
        nodes_[index].bbox = bbox;
        nodes_[index].offset = static_cast<std::uint32_t>(first);
        nodes_[index].count = static_cast<std::uint16_t>(count); // Fits: max_leaf_size is clamped to 16 bits
        ++leaf_count_;
    }

    // count stays 0 from the emplace_back, and that is what marks the node interior.
    void make_interior(std::uint32_t index, const Aabb& bbox, std::uint32_t right_child) {
        nodes_[index].bbox = bbox;
        nodes_[index].offset = right_child;
    }

    // A leaf costs one intersection test per primitive, with no traversal step - the same scale
    // best_split() reports on. The size cap is checked first: past it, a leaf is not representable.
    [[nodiscard]] bool leaf_is_cheaper(std::size_t count, Float split_cost) const noexcept {
        if (count > static_cast<std::size_t>(settings_.max_leaf_size)) return false;
        return static_cast<Float>(count) * settings_.intersection_cost <= split_cost;
    }

    // Halving a range of `count` reaches single primitives in bit_width(count - 1) levels. Once the depth
    // plus that meets the limit, an unbalanced SAH split could no longer be paid for, so from here down
    // the range is halved, which keeps every node below within the limit.
    [[nodiscard]] bool depth_budget_spent(int depth, std::size_t count) const noexcept {
        const int levels = static_cast<int>(std::bit_width(count - 1));
        return depth + levels >= settings_.depth_limit;
    }

    // Builds the subtree for info_[first, first + count) and returns its node index.
    std::uint32_t build_recursive(std::size_t first, std::size_t count, int depth) {
        // The slot is reserved before recursing, so depth-first order puts the left child at index + 1.
        const std::uint32_t index = static_cast<std::uint32_t>(nodes_.size());
        nodes_.emplace_back();
        max_depth_ = std::max(max_depth_, depth);

        // info_ is never resized during the build, so spans into it stay valid across the recursion.
        const std::span<PrimitiveInfo> prims = range(first, count);
        const Aabb bbox = bounds_of(prims);

        if (count <= 1) {
            make_leaf(index, bbox, first, count);
            return index;
        }

        std::size_t mid{};
        // Out of depth budget: a leaf if one may hold the range, otherwise halve without consulting the SAH.
        if (depth_budget_spent(depth, count)) {
            if (count <= static_cast<std::size_t>(settings_.max_leaf_size)) {
                make_leaf(index, bbox, first, count);
                return index;
            }
        } else {
            // Split on the widest axis of the centroid bounds: the axis where the primitives are most
            // spread out is the one where separating them buys the most.
            const CentroidBounds cb = centroid_bounds_of(prims);
            const auto [axis, extent] = cb.widest_axis();
            const Float node_area = bbox.surface_area();

            // A zero extent means every centroid coincides, a zero area means the box is degenerate.
            // Neither can be binned, and the area is the SAH's denominator.
            if (extent > 0 && node_area > 0) {
                const BinMapping mapping{
                    .axis = axis,
                    .min = cb.min[axis],
                    .scale = static_cast<Float>(settings_.bin_count) / extent,
                    .bin_count = settings_.bin_count};

                const SplitCandidate best = best_split(fill_bins(prims, mapping), node_area);

                if (best.is_valid()) {
                    // Stopping is a cost decision, not just a size one: max_leaf_size only bounds it.
                    if (leaf_is_cheaper(count, best.cost)) {
                        make_leaf(index, bbox, first, count);
                        return index;
                    }

                    mid = partition_by_bin(prims, mapping, best.bin);
                }
            }
        }

        // Halving fallback. Reached when the depth budget is spent, or when no SAH candidate existed at all:
        // the centroids coincide on the widest axis, the node box is degenerate, or no bin boundary separates
        // the primitives. A candidate that does exist always leaves both sides non-empty, so mid == count is
        // unreachable - the check stays as a guard on that invariant. Halving needs bit_width(count - 1)
        // levels, which keeps the recursion finite and the depth inside its budget.
        if (mid == 0 || mid == count) mid = count / 2;

        build_recursive(first, mid, depth + 1); // Lands at index + 1
        const std::uint32_t right = build_recursive(first + mid, count - mid, depth + 1);

        make_interior(index, bbox, right);
        return index;
    }
};

// Tests one leaf. A leaf is a contiguous run of primitives, which is what (offset, count) encodes.
// The interval narrows as closer hits are found, so later primitives are rejected earlier.
[[nodiscard]] bool hit_leaf(std::span<const Hittable* const> prims, const Ray& r,
                            const Interval& ray_t, HitRecord& rec) {
    bool is_hit = false;
    Float closest = ray_t.max;

    for (const Hittable* prim : prims) {
        count_leaf_test();
        if (prim->hit(r, Interval(ray_t.min, closest), rec)) {
            is_hit = true;
            closest = rec.t;
        }
    }

    return is_hit;
}

} // namespace

void BvhStats::record(const BvhTopology& topology, std::chrono::nanoseconds time) noexcept {
    build_time += time;
    ++bvh_count;
    node_count += topology.nodes.size();
    leaf_count += topology.leaf_count;
    max_depth = std::max(max_depth, topology.max_depth);
}

BvhBuildResult build_bvh_topology(std::span<const Aabb> primitive_bounds, const BvhBuildSettings& settings) {
    BvhBuildResult result = BvhBuilder(primitive_bounds, settings).build();

    // The builder caps the depth at bvh_max_depth, the traversal stack's capacity.
    // Checked because a cap that slipped by one level would overflow that stack silently.
    assert(result.topology.max_depth <= bvh_max_depth);
    return result;
}

Bvh::Bvh(std::span<const Hittable* const> objects, const BvhBuildSettings& settings) {
    std::vector<Aabb> bounds;
    bounds.reserve(objects.size());

    // Each object's bounds are read once: bounding_box() is virtual, and some objects compute rather than store it.
    std::ranges::transform(objects, std::back_inserter(bounds), [](const Hittable* obj) { return obj->bounding_box(); });

    BvhBuildResult result = build_bvh_topology(bounds, settings);
    topology_ = std::move(result.topology);
    primitives_.reserve(result.primitive_order.size());

    // The build orders indices, not objects; the tree's own primitive array is laid out from that order.
    std::ranges::transform(result.primitive_order, std::back_inserter(primitives_), [objects](std::uint32_t i) { return objects[i]; });
}

bool Bvh::hit(const Ray& r, const Interval& ray_t, HitRecord& rec) const {
    // A Hittable leaf asks each object in turn; the record is whatever the nearest one wrote.
    return bvh_closest_hit(topology_, r, ray_t, [this, &r, &rec](const BvhNode& leaf, const Interval& open) -> std::optional<Float> {
        const std::span prims = std::span(primitives_).subspan(leaf.offset, leaf.count);
        if (!hit_leaf(prims, r, open, rec)) return std::nullopt;
        return rec.t;
    });
}

Aabb Bvh::bounding_box() const { return topology_.nodes.empty() ? Aabb() : topology_.nodes[0].bbox; }

const Bvh* make_bvh(Arena<Hittable>& arena, const HittableList& list, BvhStats* stats, const BvhBuildSettings& settings) {
    // Timed around the arena call, so the measurement includes the allocation the scene pays for.
    const auto start = std::chrono::steady_clock::now();
    const Bvh* root = arena.create<Bvh>(list.objects(), settings);
    const auto end = std::chrono::steady_clock::now();

    // A scene builds one tree per mesh and group; these are its totals.
    if (stats != nullptr) stats->record(root->topology(), std::chrono::duration_cast<std::chrono::nanoseconds>(end - start));
    return root;
}

} // namespace pt
