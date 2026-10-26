#include "pt/geometry/bvh.hpp"
#include "pt/core/hit_record.hpp"
#include "pt/core/hittable.hpp"
#include "pt/geometry/mesh.hpp"
#include "pt/geometry/primitive_kind.hpp"
#include "pt/geometry/quad.hpp"
#include "pt/geometry/sphere.hpp"
#include "pt/geometry/triangle.hpp"
#include "pt/math/aabb.hpp"
#include "pt/math/constants.hpp"
#include "pt/math/interval.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include "pt/util/arena.hpp"
#include "pt/util/overloaded.hpp"
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
#include <variant>
#include <vector>

namespace pt {

namespace {

// Per-primitive build data, computed once from the caller's bounds. This array is what the build
// permutes; the primitive order is read off it at the end.
struct PrimitiveInfo {
    std::uint32_t index{};
    Aabb bbox;
    Point3 centroid;      // Binned by this, bounded by bbox - a primitive is never split
    PrimitiveKind kind{}; // Only compared: two kinds never share a leaf
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

// Whether every entry shares the first one's kind: the only ranges a leaf may hold. Non-empty only.
[[nodiscard]] bool is_single_kind(std::span<const PrimitiveInfo> prims) {
    assert(!prims.empty());

    const PrimitiveKind kind = prims.front().kind;
    return std::ranges::all_of(prims, [kind](const PrimitiveInfo& p) { return p.kind == kind; });
}

// Moves the first entry's kind to the front and returns how many there are: a split along kinds, not
// space. Stable for the reason partition_by_bin is. On a range of mixed kinds both sides are non-empty.
[[nodiscard]] std::size_t partition_by_kind(std::span<PrimitiveInfo> prims) {
    assert(!prims.empty());

    const PrimitiveKind kind = prims.front().kind;
    const auto mid = std::stable_partition(
        prims.begin(), prims.end(),
        [kind](const PrimitiveInfo& p) { return p.kind == kind; });

    return static_cast<std::size_t>(std::distance(prims.begin(), mid));
}

// Top-down BVH builder, one instance per tree. Recursion works in place on info_, narrowing to a contiguous [first, count)
// range per node; when it finishes, each leaf's primitives are adjacent, which is what lets a leaf be stored as (offset, count).
// A separate type keeps this state out of the finished topology and the public header.
class BvhBuilder {
public:
    BvhBuilder(std::span<const Aabb> primitive_bounds, std::span<const PrimitiveKind> primitive_kinds, const BvhBuildSettings& settings) : settings_(settings) {
        assert(primitive_bounds.size() <= std::numeric_limits<std::uint32_t>::max());
        assert(primitive_kinds.size() == primitive_bounds.size());

        settings_.bin_count = std::clamp(settings_.bin_count, 2, max_bin_count);
        settings_.max_leaf_size = std::clamp(settings_.max_leaf_size, 1, max_leaf_size_limit);
        settings_.depth_limit = std::clamp(settings_.depth_limit, 1, bvh_max_depth);

        info_.reserve(primitive_bounds.size());
        for (std::uint32_t i = 0; i < primitive_bounds.size(); ++i) {
            const Aabb& box = primitive_bounds[i];
            info_.push_back(PrimitiveInfo{.index = i, .bbox = box, .centroid = box.centroid(), .kind = primitive_kinds[i]});
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
        assert(count > 0);

        nodes_[index].bbox = bbox;
        nodes_[index].offset = static_cast<std::uint32_t>(first);
        nodes_[index].count = static_cast<std::uint16_t>(count); // Fits: max_leaf_size is clamped to 16 bits
        nodes_[index].kind = info_[first].kind;                  // Any entry would do: the build never makes a leaf that mixes kinds.
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
        const int levels = std::bit_width(count - 1);
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

        // Read before any reordering below, which never changes the set of kinds in the range.
        const bool single_kind = is_single_kind(prims);

        if (count <= 1) {
            make_leaf(index, bbox, first, count);
            return index;
        }

        std::size_t mid{};
        // Out of depth budget: a leaf if one may hold the range and it is of one kind, otherwise halve
        // without consulting the SAH. Halving ends in single primitives, which are always of one kind.
        if (depth_budget_spent(depth, count)) {
            if (count <= static_cast<std::size_t>(settings_.max_leaf_size) && single_kind) {
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
                        if (single_kind) {
                            make_leaf(index, bbox, first, count);
                            return index;
                        }
                        // The cost says stop, but a leaf may not mix kinds: one child per kind is the
                        // nearest the tree can come to that leaf.
                        mid = partition_by_kind(prims);
                    } else {
                        mid = partition_by_bin(prims, mapping, best.bin);
                    }
                }
            }
        }

        // Halving fallback. Reached when the depth budget is spent and no leaf may hold the range, or when
        // no SAH candidate existed at all: the centroids coincide on the widest axis, the node box is
        // degenerate, or no bin boundary separates the primitives. A candidate that does exist always leaves
        // both sides non-empty, so mid == count is unreachable - the check stays as a guard on that invariant.
        // Halving needs bit_width(count - 1) levels, which keeps the recursion finite and the depth inside its budget.
        if (mid == 0 || mid == count) mid = count / 2;

        build_recursive(first, mid, depth + 1); // Lands at index + 1
        const std::uint32_t right = build_recursive(first + mid, count - mid, depth + 1);

        make_interior(index, bbox, right);
        return index;
    }
};

// How a leaf reaches one kind's stored shapes: the geometric test, and the distance its hit carries.
// Specialised per stored shape; the leaf scan below is written against this and nothing else.
template <typename Shape>
struct ShapeTraits;

// A sphere's hit is its distance; the fill rebuilds the rest
template <>
struct ShapeTraits<SphereShape> {
    using Hit = Float;

    [[nodiscard]] static bool intersect(const Ray& r, const Interval& ray_t, const SphereShape& shape, Hit& out) noexcept {
        const std::optional<Float> t = intersect_sphere(r, ray_t, shape);
        if (!t) return false;
        out = *t;
        return true;
    }

    [[nodiscard]] static Float distance(const Hit& hit) noexcept { return hit; }
};

template <>
struct ShapeTraits<QuadShape> {
    using Hit = QuadHit;

    [[nodiscard]] static bool intersect(const Ray& r, const Interval& ray_t, const QuadShape& shape, Hit& out) noexcept {
        return intersect_quad(r, ray_t, shape, out);
    }

    [[nodiscard]] static Float distance(const Hit& hit) noexcept { return hit.t; }
};

template <>
struct ShapeTraits<TriangleCorners> {
    using Hit = TriangleHit;

    [[nodiscard]] static bool intersect(const Ray& r, const Interval& ray_t, const TriangleCorners& shape, Hit& out) noexcept {
        return intersect_triangle(r, ray_t, shape.v0, shape.v1, shape.v2, out);
    }

    [[nodiscard]] static Float distance(const Hit& hit) noexcept { return hit.t; }
};

// The nearest candidate of one kind so far: its slot in that kind's arrays, and what its test returned.
template <typename Shape>
struct ShapeWinner {
    std::uint32_t slot{};
    typename ShapeTraits<Shape>::Hit hit{};
};

// Tests one leaf's run inside `open`, narrowing as it goes. Every candidate that narrows overwrites `winner`,
// so the last write is the leaf's nearest; returns its distance, or nothing.
template <typename Shape>
[[nodiscard]] std::optional<Float> closest_in_leaf(std::span<const Shape> shapes, const BvhNode& leaf, const Ray& r,
                                                   const Interval& open, ShapeWinner<Shape>& winner) noexcept {
    Float closest = open.max;
    bool leaf_hit{false};

    const std::uint32_t end = leaf.offset + static_cast<std::uint32_t>(leaf.count);
    for (std::uint32_t slot = leaf.offset; slot < end; ++slot) {
        count_leaf_test();

        typename ShapeTraits<Shape>::Hit candidate{};
        if (ShapeTraits<Shape>::intersect(r, Interval(open.min, closest), shapes[slot], candidate)) {
            closest = ShapeTraits<Shape>::distance(candidate);
            winner = ShapeWinner<Shape>{.slot = slot, .hit = candidate};
            leaf_hit = true;
        }
    }

    return leaf_hit ? std::optional<Float>(closest) : std::nullopt;
}

// Tests one leaf. A leaf is a contiguous run of primitives, which is what (offset, count) encodes.
// The interval narrows as closer hits are found, so later primitives are rejected earlier. The child fills the record itself.
[[nodiscard]] std::optional<Float> closest_in_opaque_leaf(std::span<const Hittable* const> prims, const Ray& r, const Interval& ray_t, HitRecord& rec) {
    bool is_hit = false;
    Float closest = ray_t.max;

    for (const Hittable* prim : prims) {
        count_leaf_test();
        if (prim->hit(r, Interval(ray_t.min, closest), rec)) {
            is_hit = true;
            closest = rec.t;
        }
    }

    return is_hit ? std::optional<Float>(closest) : std::nullopt;
}

[[nodiscard]] PrimitiveKind kind_of(const PrimitiveRef& primitive) {
    // clang-format off
    return std::visit(Overloaded{
        [](const Sphere*)   { return PrimitiveKind::sphere; },
        [](const Quad*)     { return PrimitiveKind::quad;   },
        [](const Hittable*) { return PrimitiveKind::opaque; },
    }, primitive);
    // clang-format on
}

// The bounds each object reports for itself, from the same shape: the tree is the one a build over the objects would get.
[[nodiscard]] Aabb primitive_bounds(const PrimitiveRef& primitive) {
    // clang-format off
    return std::visit(Overloaded{
        [](const Sphere* s)   { return sphere_bounds(s->shape()); },
        [](const Quad* q)     { return quad_bounds(q->shape());   },
        [](const Hittable* h) { return h->bounding_box();         },
    }, primitive);
    // clang-format on
}

// Timed around the arena call: bounds, build, gather and allocation are all part of what the scene pays.
template <typename Source>
[[nodiscard]] const Bvh* create_timed(Arena<Hittable>& arena, BvhStats* stats, const Source& source, const BvhBuildSettings& settings) {
    const auto start = std::chrono::steady_clock::now();
    const Bvh* root = arena.create<Bvh>(source, settings);
    const auto end = std::chrono::steady_clock::now();

    // A scene builds one tree per mesh and group; these are its totals.
    if (stats != nullptr) stats->record(root->topology(), std::chrono::duration_cast<std::chrono::nanoseconds>(end - start));
    return root;
}

} // namespace

void BvhStats::record(const BvhTopology& topology, std::chrono::nanoseconds time) noexcept {
    build_time += time;
    ++bvh_count;
    node_count += topology.nodes.size();
    leaf_count += topology.leaf_count;
    max_depth = std::max(max_depth, topology.max_depth);
}

BvhBuildResult build_bvh_topology(std::span<const Aabb> primitive_bounds, std::span<const PrimitiveKind> primitive_kinds, const BvhBuildSettings& settings) {
    BvhBuildResult result = BvhBuilder(primitive_bounds, primitive_kinds, settings).build();

    // The builder caps the depth at bvh_max_depth, the traversal stack's capacity.
    // Checked because a cap that slipped by one level would overflow that stack silently.
    assert(result.topology.max_depth <= bvh_max_depth);
    return result;
}

BvhBuildResult build_bvh_topology(std::span<const Aabb> primitive_bounds, PrimitiveKind kind, const BvhBuildSettings& settings) {
    // One label per primitive keeps a single build path; the cost is a byte per primitive while it runs.
    std::vector<PrimitiveKind> kinds(primitive_bounds.size(), kind);
    return build_bvh_topology(primitive_bounds, kinds, settings);
}

Bvh::Bvh(std::span<const PrimitiveRef> primitives, const BvhBuildSettings& settings) {
    assert(primitives.size() <= std::numeric_limits<std::uint32_t>::max());

    std::vector<Aabb> bounds;
    std::vector<PrimitiveKind> kinds;
    bounds.reserve(primitives.size());
    kinds.reserve(primitives.size());

    for (const PrimitiveRef& p : primitives) {
        bounds.push_back(primitive_bounds(p));
        kinds.push_back(kind_of(p));
    }

    BvhBuildResult result = build_bvh_topology(bounds, kinds, settings);
    topology_ = std::move(result.topology);

    const auto count_of = [&kinds](PrimitiveKind kind) {
        return static_cast<std::size_t>(std::ranges::count(kinds, kind));
    };
    spheres_.shapes.reserve(count_of(PrimitiveKind::sphere));
    spheres_.materials.reserve(count_of(PrimitiveKind::sphere));
    quads_.shapes.reserve(count_of(PrimitiveKind::quad));
    quads_.materials.reserve(count_of(PrimitiveKind::quad));
    opaque_.reserve(count_of(PrimitiveKind::opaque));

    // Gathered in leaf order, each kind into its own arrays. A slot's index within its kind is recorded as it lands.
    std::vector<std::uint32_t> local_index;
    local_index.reserve(primitives.size());

    for (const std::uint32_t id : result.primitive_order) {
        // clang-format off
        std::visit(Overloaded{
            [&](const Sphere* s) {
                local_index.push_back(static_cast<std::uint32_t>(spheres_.shapes.size()));
                spheres_.shapes.push_back(s->shape());
                spheres_.materials.push_back(s->material());
            },
            [&](const Quad* q) {
                local_index.push_back(static_cast<std::uint32_t>(quads_.shapes.size()));
                quads_.shapes.push_back(q->shape());
                quads_.materials.push_back(q->material());
            },
            [&](const Hittable* h) {
                local_index.push_back(static_cast<std::uint32_t>(opaque_.size()));
                opaque_.push_back(h);
            },
        }, primitives[id]);
        // clang-format on
    }

    // The build numbers slots across every kind. A leaf is one kind's contiguous run, so its first slot's index
    // within that kind is where the run starts in that kind's arrays. With a single kind this changes nothing.
    for (BvhNode& node : topology_.nodes) {
        if (node.is_leaf()) node.offset = local_index[node.offset];
    }
}

Bvh::Bvh(const Mesh& mesh, const BvhBuildSettings& settings) {
    assert(mesh.triangle_count() <= std::numeric_limits<std::uint32_t>::max());

    std::vector<Aabb> bounds;
    bounds.reserve(mesh.triangle_count());

    // The same bounds a per-triangle object would report, in the same order, so the tree is the one it would get.
    for (std::size_t i = 0; i < mesh.triangle_count(); ++i) {
        const auto [a, b, c] = mesh.triangle(i);
        bounds.push_back(triangle_bounds(a, b, c));
    }

    BvhBuildResult result = build_bvh_topology(bounds, PrimitiveKind::triangle, settings);
    topology_ = std::move(result.topology);
    triangles_.ids = std::move(result.primitive_order);
    triangles_.mesh = &mesh;

    triangles_.corners.reserve(triangles_.ids.size());

    // Gathered in leaf order: this copy is what the intersection test reads, the mesh's buffers never.
    for (const std::uint32_t id : triangles_.ids) {
        const auto [a, b, c] = mesh.triangle(id);
        triangles_.corners.push_back(TriangleCorners{.v0 = a, .v1 = b, .v2 = c});
    }

    // One kind throughout, so a slot is already its index in the triangle arrays: nothing to rebase.
}

bool Bvh::hit(const Ray& r, const Interval& ray_t, HitRecord& rec) const {
    // One winner per kind rather than one shared variant: each kernel writes only its own. A leaf reports only a
    // candidate nearer than every earlier one, so `last`, the kind of the latest report, names the overall winner.
    ShapeWinner<SphereShape> sphere;
    ShapeWinner<QuadShape> quad;
    ShapeWinner<TriangleCorners> triangle;
    PrimitiveKind last{};

    const bool found = bvh_closest_hit(
        topology_, r, ray_t,
        [this, &r, &rec, &sphere, &quad, &triangle, &last](
            const BvhNode& leaf, const Interval& open) -> std::optional<Float> {
            std::optional<Float> t;

            switch (leaf.kind) {
            case PrimitiveKind::sphere:
                t = closest_in_leaf<SphereShape>(
                    spheres_.shapes, leaf, r, open, sphere);
                break;
            case PrimitiveKind::quad:
                t = closest_in_leaf<QuadShape>(
                    quads_.shapes, leaf, r, open, quad);
                break;
            case PrimitiveKind::triangle:
                t = closest_in_leaf<TriangleCorners>(
                    triangles_.corners, leaf, r, open, triangle);
                break;
            case PrimitiveKind::opaque:
                t = closest_in_opaque_leaf(
                    std::span(opaque_).subspan(leaf.offset, leaf.count),
                    r, open, rec);
                break;
            }

            if (t) last = leaf.kind;
            return t;
        });

    if (!found) return false;

    // Written once, for the hit that survived: a miss leaves the caller's record untouched.
    switch (last) {
    case PrimitiveKind::sphere:
        fill_sphere_record(r, spheres_.shapes[sphere.slot], sphere.hit, spheres_.materials[sphere.slot], rec);
        break;
    case PrimitiveKind::quad:
        fill_quad_record(r, quads_.shapes[quad.slot], quad.hit, quads_.materials[quad.slot], rec);
        break;
    case PrimitiveKind::triangle:
        fill_triangle_record(r, triangle.slot, triangle.hit, rec);
        break;
    case PrimitiveKind::opaque:
        // The child wrote the record when it won; a nearer winner of another kind would have taken `last`.
        break;
    }

    return true;
}

Aabb Bvh::bounding_box() const { return topology_.nodes.empty() ? Aabb() : topology_.nodes[0].bbox; }

void Bvh::fill_triangle_record(const Ray& r, std::uint32_t slot, const TriangleHit& hit, HitRecord& rec) const noexcept {
    const TriangleCorners& tri = triangles_.corners[slot];
    const std::uint32_t id = triangles_.ids[slot];
    const Uv uv = triangles_.mesh->texture_uv(id, hit.b1, hit.b2);

    rec.t = hit.t;
    rec.p = barycentric_point(tri.v0, tri.v1, tri.v2, hit.b1, hit.b2);
    rec.spawn_margin = 0.0_f; // Written explicitly: the caller reuses records across queries.
    rec.u = uv.u;
    rec.v = uv.v;
    rec.mat = triangles_.mesh->material();
    rec.set_face_normal(r, hit.normal, triangles_.mesh->shading_normal(id, hit.b1, hit.b2, hit.normal));
}

const Bvh* make_bvh(Arena<Hittable>& arena, std::span<const PrimitiveRef> primitives, BvhStats* stats, const BvhBuildSettings& settings) {
    return create_timed(arena, stats, primitives, settings);
}

const Bvh* make_mesh_bvh(Arena<Hittable>& arena, const Mesh& mesh, BvhStats* stats, const BvhBuildSettings& settings) {
    return create_timed(arena, stats, mesh, settings);
}

} // namespace pt
