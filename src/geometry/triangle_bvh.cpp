#include "pt/geometry/triangle_bvh.hpp"
#include "pt/core/hit_record.hpp"
#include "pt/core/hittable.hpp"
#include "pt/geometry/bvh.hpp"
#include "pt/geometry/mesh.hpp"
#include "pt/geometry/triangle.hpp"
#include "pt/math/aabb.hpp"
#include "pt/math/interval.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/scalar.hpp"
#include "pt/util/arena.hpp"
#include "pt/util/stats.hpp"
#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

namespace pt {

TriangleBvh::TriangleBvh(const Mesh& mesh, const BvhBuildSettings& settings) : mesh_(&mesh) {
    assert(mesh.triangle_count() <= std::numeric_limits<std::uint32_t>::max());

    std::vector<Aabb> bounds;
    bounds.reserve(mesh.triangle_count());

    // The same bounds a per-triangle object would report, in the same order, so the tree is the one it would get.
    for (std::size_t i = 0; i < mesh.triangle_count(); ++i) {
        const auto [a, b, c] = mesh.triangle(i);
        bounds.push_back(triangle_bounds(a, b, c));
    }

    BvhBuildResult result = build_bvh_topology(bounds, settings);
    topology_ = std::move(result.topology);
    triangle_ids_ = std::move(result.primitive_order);

    triangles_.reserve(triangle_ids_.size());

    // Gathered in leaf order: this copy is what the intersection test reads, the mesh's buffers never.
    for (const std::uint32_t id : triangle_ids_) {
        const auto [a, b, c] = mesh.triangle(id);
        triangles_.push_back(Corners{.v0 = a, .v1 = b, .v2 = c});
    }
}

bool TriangleBvh::hit(const Ray& r, const Interval& ray_t, HitRecord& rec) const {
    std::uint32_t best_slot{};
    TriangleHit best{};

    // Leaves run the geometric test only and remember the winner. A leaf searches only the interval traversal
    // hands it, which already ends at the nearest hit so far, so the last winner kept is the nearest overall.
    const bool found = bvh_closest_hit(
        topology_, r, ray_t, [this, &r, &best_slot, &best](const BvhNode& leaf, const Interval& open) -> std::optional<Float> {
            Float closest = open.max;
            bool leaf_hit{false};

            const std::uint32_t end = leaf.offset + static_cast<std::uint32_t>(leaf.count);
            for (std::uint32_t slot = leaf.offset; slot < end; ++slot) {
                count_leaf_test();

                const Corners& tri = triangles_[slot];
                TriangleHit candidate;
                if (intersect_triangle(r, Interval(open.min, closest), tri.v0, tri.v1, tri.v2, candidate)) {
                    closest = candidate.t;
                    best_slot = slot;
                    best = candidate;
                    leaf_hit = true;
                }
            }

            return leaf_hit ? std::optional<Float>(closest) : std::nullopt;
        });

    if (!found) return false;

    // Written once, for the hit that survived: a miss leaves the caller's record untouched.
    fill_record(r, best_slot, best, rec);
    return true;
}

Aabb TriangleBvh::bounding_box() const { return topology_.nodes.empty() ? Aabb() : topology_.nodes[0].bbox; }

void TriangleBvh::fill_record(const Ray& r, std::uint32_t slot, const TriangleHit& hit, HitRecord& rec) const noexcept {
    const Corners& tri = triangles_[slot];
    const std::uint32_t id = triangle_ids_[slot];
    const Uv uv = mesh_->texture_uv(id, hit.b1, hit.b2);

    rec.t = hit.t;
    rec.p = barycentric_point(tri.v0, tri.v1, tri.v2, hit.b1, hit.b2);
    rec.spawn_margin = 0.0_f; // Written explicitly: the caller reuses records across queries.
    rec.u = uv.u;
    rec.v = uv.v;
    rec.mat = mesh_->material();
    rec.set_face_normal(r, hit.normal, mesh_->shading_normal(id, hit.b1, hit.b2, hit.normal));
}

const TriangleBvh* make_triangle_bvh(Arena<Hittable>& arena, const Mesh& mesh, BvhStats* stats, const BvhBuildSettings& settings) {
    // Timed around the arena call: bounds, build and gather are all part of what the scene pays.
    const auto start = std::chrono::steady_clock::now();
    const TriangleBvh* tree = arena.create<TriangleBvh>(mesh, settings);
    const auto end = std::chrono::steady_clock::now();

    // A scene builds one tree per mesh and group; these are its totals.
    if (stats != nullptr) stats->record(tree->topology(), std::chrono::duration_cast<std::chrono::nanoseconds>(end - start));
    return tree;
}

} // namespace pt
