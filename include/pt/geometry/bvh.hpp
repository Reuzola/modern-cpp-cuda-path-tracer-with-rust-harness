#pragma once
#include "pt/core/hit_record.hpp"
#include "pt/core/hittable.hpp"
#include "pt/geometry/bvh_node.hpp"
#include "pt/geometry/primitive_kind.hpp"
#include "pt/geometry/primitive_store.hpp"
#include "pt/geometry/quad.hpp"
#include "pt/geometry/sphere.hpp"
#include "pt/math/aabb.hpp"
#include "pt/math/interval.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include "pt/util/arena.hpp"
#include "pt/util/stats.hpp"
#include <array>
#include <chrono>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <type_traits>
#include <variant>
#include <vector>

namespace pt {

namespace detail {

// A deferred far child, already box-tested by its parent. `t_enter` is geometric and never changes,
// so a single comparison against the narrowed `closest` reproduces the full slab test's verdict.
struct TraversalEntry {
    std::uint32_t index{};
    Float t_enter{};
};

} // namespace detail

class Mesh;

// Deepest node any tree may have; the root is depth 0. It sizes the traversal stack, which holds at
// most one deferred child per level, and the builder never exceeds it, so no input can overflow it.
inline constexpr int bvh_max_depth = 64;

// Binned SAH build parameters (cost ratio determines tree shape).
// Integer fields are clamped: bin_count in [2, 32], max_leaf_size in [1, 65535], depth_limit in [1, bvh_max_depth].
struct BvhBuildSettings {
    Float traversal_cost{1.0_f};
    Float intersection_cost{8.0_f};
    int bin_count{12};
    int max_leaf_size{4};

    // Cap on node depth. A node that could no longer afford an SAH split under it is halved instead,
    // so a tree over n primitives is at most max(depth_limit, ceil(log2 n)) deep.
    int depth_limit{bvh_max_depth};
};

// The tree without its primitives: what a build produces and traversal walks. Its owner keeps the
// primitives in the build's order, which is what lets a leaf's (offset, count) name a contiguous run.
struct BvhTopology {
    std::vector<BvhNode> nodes; // Depth first; nodes[0] is the root when non-empty
    std::size_t leaf_count{};
    int max_depth{};
};

// Scene-wide BVH statistics accumulated across all meshes, groups, and root trees.
struct BvhStats {
    std::size_t bvh_count{};               // Trees folded in so far
    std::size_t node_count{};              // Interior and leaf nodes, summed over all trees
    std::size_t leaf_count{};              // Leaf nodes, summed over all trees
    int max_depth{};                       // Deepest node of any tree (each tree's root is depth 0)
    std::chrono::nanoseconds build_time{}; // Summed over all trees; integer ns, so lossless

    // Folds one finished tree in. Counts add across trees; depth is the deepest of any.
    void record(const BvhTopology& topology, std::chrono::nanoseconds time) noexcept;
};

struct BvhBuildResult {
    BvhTopology topology;
    std::vector<std::uint32_t> primitive_order; // Leaf slot k holds input primitive primitive_order[k]
};

// A leaf test: handed a leaf and the interval still open, it reports the nearest hit it found inside
// that interval, or nothing. What a hit records is the test's own business; traversal only narrows by t.
template <typename F>
concept BvhLeafTest = std::invocable<F&, const BvhNode&, const Interval&> &&
                      std::same_as<std::invoke_result_t<F&, const BvhNode&, const Interval&>, std::optional<Float>>;

// A primitive as a tree is built from it, typed by the kind it takes in a leaf. Read during the build only:
// a sphere or a quad is copied in, anything else is an opaque child kept by pointer and hit through its own hit().
using PrimitiveRef = std::variant<const Sphere*, const Quad*, const Hittable*>;

// Iterative, distance-ordered traversal. The near child is descended into directly and the far one
// is deferred with its entry distance, so a hit found in the near subtree can reject it later with a
// single comparison instead of a repeated slab test. Shared by every tree; only the leaf differs.
// Returns the nearest distance a leaf reported, or nothing: a caller one level up narrows by it.
template <BvhLeafTest LeafTest>
[[nodiscard]] std::optional<Float> bvh_closest_hit(const BvhTopology& topology, const Ray& r, const Interval& ray_t, LeafTest leaf_test) {
    const std::span<const BvhNode> nodes = topology.nodes;
    if (nodes.empty()) return std::nullopt;

    // Divided once per query, not once per node. It must live here rather than in Ray: Instance builds a
    // transformed ray per hit, and nested trees (meshes, groups) each see a different one.
    const Vec3 inv_dir(1.0_f / r.direction().x(), 1.0_f / r.direction().y(), 1.0_f / r.direction().z());
    Float closest{ray_t.max}; // Narrowing upper bound; the parameter the recursion used to carry
    bool is_hit{false};

    // Tested outside the loop to establish the invariant the body relies on: `current` always names a
    // node whose box has already been tested and passed.
    count_node_test();
    if (!nodes[0].bbox.intersect(r.origin(), inv_dir, Interval(ray_t.min, closest))) return std::nullopt;

    // Fixed size and local: hit() is const and will be shared across threads, so no traversal state
    // may live in the object. Capacity is guaranteed by the builder's depth cap, bvh_max_depth.
    std::array<detail::TraversalEntry, bvh_max_depth> stack;
    std::size_t stack_size{};
    std::uint32_t current{};

    while (true) {
        const BvhNode& node = nodes[current];
        if (node.is_leaf()) {
            // The whole leaf is the test's to consume; then fall through to the pop step.
            if (const std::optional<Float> t = leaf_test(node, Interval(ray_t.min, closest))) {
                is_hit = true;
                closest = *t;
            }
        } else { // interior branch
            // Left child is implicit at index + 1 (depth-first layout); the right index is what the node stores.
            const std::uint32_t left = current + 1;
            const std::uint32_t right = node.offset;

            count_node_test();
            const std::optional<Float> t_left = nodes[left].bbox.intersect(r.origin(), inv_dir, Interval(ray_t.min, closest));
            count_node_test();
            const std::optional<Float> t_right = nodes[right].bbox.intersect(r.origin(), inv_dir, Interval(ray_t.min, closest));

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
            const detail::TraversalEntry entry = stack[--stack_size];
            if (entry.t_enter <= closest) {
                current = entry.index;
                found = true;
                break;
            }
        }

        if (!found) return is_hit ? std::optional<Float>(closest) : std::nullopt;
    }
}

// Read-only BVH built via binned SAH. A leaf holds primitives of one kind, laid out in leaf order in that
// kind's own arrays, so testing it is one switch and a loop over contiguous memory. Spheres and quads are
// copied in; a mesh's triangles have their corners copied and their attributes read from the mesh; opaque
// children are kept by pointer. The mesh and the opaque children must outlive the tree.
class Bvh final : public Hittable {
public:
    // Any mix of spheres, quads and opaque children. `primitives` is read during construction only.
    explicit Bvh(std::span<const PrimitiveRef> primitives, const BvhBuildSettings& settings = {});

    // Every triangle of `mesh`, which a hit reads its attributes from.
    explicit Bvh(const Mesh& mesh, const BvhBuildSettings& settings = {});

    // Closest hit inside ray_t, or false when the ray misses the tree entirely.
    [[nodiscard]] bool hit(const Ray& r, const Interval& ray_t, HitRecord& rec) const override;

    // Root bounds, or an empty box when the tree holds no primitives.
    [[nodiscard]] Aabb bounding_box() const override;

    // The three counters below exist for reporting; traversal never reads them.
    [[nodiscard]] std::size_t node_count() const noexcept { return topology_.nodes.size(); }

    [[nodiscard]] std::size_t leaf_count() const noexcept { return topology_.leaf_count; }

    [[nodiscard]] int max_depth() const noexcept { return topology_.max_depth; }

    [[nodiscard]] const BvhTopology& topology() const noexcept { return topology_; }

private:
    BvhTopology topology_;
    PrimitiveStore store_; // Every leaf of a kind the store holds; an opaque leaf indexes opaque_ instead.
    std::vector<const Hittable*> opaque_;
};

/// Builds a binned SAH tree over primitives known by their bounds and kind: entry i of both spans is
/// primitive i. No leaf mixes kinds, and each leaf records the one it holds. Kinds are labels here, only
/// ever compared. The result names no primitive type; the caller lays its primitives out in primitive_order.
[[nodiscard]] BvhBuildResult build_bvh_topology(std::span<const Aabb> primitive_bounds,
                                                std::span<const PrimitiveKind> primitive_kinds, const BvhBuildSettings& settings = {});

/// The same build over primitives that are all of one kind.
[[nodiscard]] BvhBuildResult build_bvh_topology(std::span<const Aabb> primitive_bounds, PrimitiveKind kind,
                                                const BvhBuildSettings& settings = {});

/// Points every leaf at its run in its own kind's arrays. A build numbers slots across all kinds; once its
/// primitives are laid out kind by kind in leaf order, `kind_slot[k]` is where the k-th landed within its kind.
void rebase_leaf_offsets(BvhTopology& topology, std::span<const std::uint32_t> kind_slot) noexcept;

/// Builds a tree over `primitives` and stores it in `arena`, which owns it.
/// `primitives` is not retained; opaque children must outlive the arena.
/// @param stats If non-null, this tree's counts and build time are folded into it.
[[nodiscard]] const Bvh* make_bvh(Arena<Hittable>& arena, std::span<const PrimitiveRef> primitives,
                                  BvhStats* stats = nullptr, const BvhBuildSettings& settings = {});

/// The same for every triangle of `mesh`, which must outlive the arena.
[[nodiscard]] const Bvh* make_mesh_bvh(Arena<Hittable>& arena, const Mesh& mesh,
                                       BvhStats* stats = nullptr, const BvhBuildSettings& settings = {});

} // namespace pt
