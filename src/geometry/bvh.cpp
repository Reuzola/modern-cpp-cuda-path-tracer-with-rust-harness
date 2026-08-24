#include "pt/geometry/bvh.hpp"
#include "pt/core/hit_record.hpp"
#include "pt/core/hittable.hpp"
#include "pt/core/hittable_list.hpp"
#include "pt/math/aabb.hpp"
#include "pt/math/constants.hpp"
#include "pt/math/interval.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include "pt/util/arena.hpp"
#include "pt/util/stats.hpp"
#include <algorithm>
#include <array>
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

// Per-primitive build data, computed once up front. Hittable::bounding_box() is virtual and
// MeshTriangle recomputes it from vertices on every call, so the build must not ask twice.
// This array is what the build permutes; the primitive pointers are extracted from it at the end.
struct PrimitiveInfo {
    const Hittable* primitive = nullptr;
    Aabb bbox;
    Point3 centroid; // Binned by this, bounded by bbox - a primitive is never split
};

// Widest axis of a centroid bounds, and its size.
struct AxisExtent {
    int axis{};
    Float extent{};
};

// Centroid bounds, tracked outside Aabb on purpose: Aabb's point ctor pads degenerate axes by 1e-4,
// which would hide the "all centroids coincide" case that the binning split must detect.
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

// What the builder hands back to Bvh, moved out wholesale. Build-time scratch state - info_ and the
// settings - stays inside the builder and is destroyed with it.
struct BvhBuildResult {
    std::vector<BvhNode> nodes;
    std::vector<const Hittable*> primitives;
    std::size_t leaf_count{};
    int max_depth{};
};

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
// A separate type keeps this state out of both the finished Bvh and the public header.
class BvhBuilder {
public:
    BvhBuilder(std::span<const Hittable* const> objects, const BvhBuildSettings& settings) : settings_(settings) {
        settings_.bin_count = std::clamp(settings_.bin_count, 2, max_bin_count);
        settings_.max_leaf_size = std::clamp(settings_.max_leaf_size, 1, max_leaf_size_limit);

        info_.reserve(objects.size());
        for (const Hittable* obj : objects) {
            const Aabb box = obj->bounding_box();
            info_.push_back(PrimitiveInfo{.primitive = obj, .bbox = box, .centroid = box.centroid()});
        }

        // 2N-1 is the node count with one primitive per leaf, so it upper-bounds any larger-leaf tree.
        if (!info_.empty()) nodes_.reserve(2 * info_.size() - 1);
    }

    // Consumes the builder: info_ is left permuted and nodes_ moved from.
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

        // Split on the widest axis of the centroid bounds: the axis where the primitives are most
        // spread out is the one where separating them buys the most.
        const CentroidBounds cb = centroid_bounds_of(prims);
        const auto [axis, extent] = cb.widest_axis();

        std::size_t mid{};
        const Float node_area = bbox.surface_area();

        // A zero extent means every centroid coincides, a zero area means the box is degenerate.
        // Neither can be binned, and the area is the SAH's denominator.
        if (extent > 0 && node_area > 0) {
            const BinMapping mapping{.axis = axis,
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

        // Median fallback, reached only when no SAH candidate existed at all: the centroids
        // coincide on the widest axis, the node box is degenerate, or no bin boundary separates
        // the primitives. A candidate that does exist always leaves both sides non-empty, so
        // mid == count is unreachable - the check stays as a guard on that invariant. The tree is
        // worse than a SAH split, but the recursion is guaranteed to terminate.
        if (mid == 0 || mid == count) mid = count / 2;

        build_recursive(first, mid, depth + 1); // Lands at index + 1
        const std::uint32_t right = build_recursive(first + mid, count - mid, depth + 1);

        make_interior(index, bbox, right);
        return index;
    }
};

// Traversal pushes at most one entry per ancestor on the current root-to-leaf path, so the tree's
// max depth bounds the stack. Deepest scene today is 13; 64 matches the industry default and
// leaves room for meshes orders of magnitude larger.
constexpr int max_traversal_depth = 64;

// A deferred far child, already box-tested by its parent. `t_enter` is geometric and never changes,
// so a single comparison against the narrowed `closest` reproduces the full slab test's verdict.
struct TraversalEntry {
    std::uint32_t index{};
    Float t_enter{};
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

Bvh::Bvh(std::span<const Hittable* const> objects, const BvhBuildSettings& settings) {
    // Assigned in the body, not the init list: all four members come out of one build pass, and
    // BvhBuildResult lives in this file's anonymous namespace so no ctor can name it.
    BvhBuildResult result = BvhBuilder(objects, settings).build();

    nodes_ = std::move(result.nodes);
    primitives_ = std::move(result.primitives);
    leaf_count_ = result.leaf_count;
    max_depth_ = result.max_depth;

    // The traversal stack is fixed size; a tree deeper than it would overflow.
    assert(max_depth_ <= max_traversal_depth);
}

// Iterative, distance-ordered traversal. The near child is descended into directly and the far one
// is deferred with its entry distance, so a hit found in the near subtree can reject it later with a
// single comparison instead of a repeated slab test.
bool Bvh::hit(const Ray& r, const Interval& ray_t, HitRecord& rec) const {
    if (nodes_.empty()) return false;

    // Divided once per query, not once per node. It must live here rather than in Ray: Instance builds a
    // transformed ray per hit, and nested trees (meshes, groups) each see a different one.
    const Vec3 inv_dir(1.0_f / r.direction().x(), 1.0_f / r.direction().y(), 1.0_f / r.direction().z());
    Float closest{ray_t.max}; // Narrowing upper bound; the parameter the recursion used to carry
    bool is_hit{false};

    // Tested outside the loop to establish the invariant the body relies on: `current` always names a
    // node whose box has already been tested and passed.
    count_node_test();
    if (!nodes_[0].bbox.intersect(r.origin(), inv_dir, Interval(ray_t.min, closest))) return false;

    // Fixed size and local: hit() is const and will be shared across threads, so no traversal state
    // may live in the object. Capacity is guaranteed by the depth assert in the constructor.
    std::array<TraversalEntry, max_traversal_depth> stack;
    std::size_t stack_size{};
    std::uint32_t current{};

    while (true) {
        const BvhNode& node = nodes_[current];
        if (node.is_leaf()) {
            // A leaf is a contiguous primitive range, so the whole leaf is consumed here; then fall through to the pop step.
            const std::span<const Hittable* const> leaf_prims = std::span(primitives_).subspan(node.offset, node.count);
            if (hit_leaf(leaf_prims, r, Interval(ray_t.min, closest), rec)) {
                is_hit = true;
                closest = rec.t;
            }
        } else { // interior branch
            // Left child is implicit at index + 1 (depth-first layout); the right index is what the node stores.
            const std::uint32_t left = current + 1;
            const std::uint32_t right = node.offset;

            count_node_test();
            const std::optional<Float> t_left = nodes_[left].bbox.intersect(r.origin(), inv_dir, Interval(ray_t.min, closest));
            count_node_test();
            const std::optional<Float> t_right = nodes_[right].bbox.intersect(r.origin(), inv_dir, Interval(ray_t.min, closest));

            if (t_left && t_right) {
                // Ties go left, matching the build's ordering and keeping the traversal deterministic.
                const bool left_is_near = *t_left <= *t_right;
                const std::uint32_t near_child = left_is_near ? left : right;
                const std::uint32_t far_child = left_is_near ? right : left;
                const Float far_t = left_is_near ? *t_right : *t_left;

                current = near_child;
                stack[stack_size++] = {.index = far_child, .t_enter = far_t};
                continue;
            }
            if (t_left) {
                current = left;
                continue;
            }
            if (t_right) {
                current = right;
                continue;
            }
        }

        // Pops until an entry survives the narrowed bound. More than one can have gone stale, since every
        // hit since they were pushed lowered `closest`.
        bool found{false};
        while (stack_size > 0) {
            const TraversalEntry entry = stack[--stack_size];
            if (entry.t_enter <= closest) {
                current = entry.index;
                found = true;
                break;
            }
        }

        if (!found) return is_hit;
    }
}

Aabb Bvh::bounding_box() const { return nodes_.empty() ? Aabb() : nodes_[0].bbox; }

const Bvh* make_bvh(Arena<Hittable>& arena, const HittableList& list, BvhStats* stats, const BvhBuildSettings& settings) {
    // Timed around the arena call, so the measurement includes the allocation the scene pays for.
    const auto start = std::chrono::steady_clock::now();
    const Bvh* root = arena.create<Bvh>(list.objects(), settings);
    const auto end = std::chrono::steady_clock::now();

    // Folded in, not assigned: a scene builds one tree per mesh and group, and these are the totals.
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
