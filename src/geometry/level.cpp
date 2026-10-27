#include "pt/geometry/level.hpp"
#include "pt/geometry/bvh.hpp"
#include "pt/geometry/instance.hpp"
#include "pt/geometry/mesh.hpp"
#include "pt/geometry/primitive_kind.hpp"
#include "pt/geometry/quad.hpp"
#include "pt/geometry/sphere.hpp"
#include "pt/geometry/triangle.hpp"
#include "pt/math/aabb.hpp"
#include "pt/util/overloaded.hpp"
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <span>
#include <utility>
#include <variant>
#include <vector>

namespace pt {

LevelBuild build_level(std::span<const LevelItem> items, std::span<const Aabb> bvh_bounds, const BvhBuildSettings& settings) {
    std::vector<Aabb> bounds;
    std::vector<PrimitiveKind> kinds;
    std::vector<std::uint32_t> first; // first[i]: the build id of item i's first primitive
    first.reserve(items.size());

    std::size_t spheres{};
    std::size_t quads{};
    std::size_t triangles{};
    std::size_t instances{};

    // Expanded in item order, a mesh's triangles in its own order, so the tree is the one the same
    // primitives would build however they were grouped.
    for (const LevelItem& item : items) {
        first.push_back(static_cast<std::uint32_t>(bounds.size()));

        // clang-format off
        std::visit(Overloaded{
            [&](const Sphere* s) {
                bounds.push_back(sphere_bounds(s->shape()));
                kinds.push_back(PrimitiveKind::sphere);
                ++spheres;
            }, [&](const Quad* q) {
                bounds.push_back(quad_bounds(q->shape()));
                kinds.push_back(PrimitiveKind::quad);
                ++quads;
            }, [&](const Mesh* m) {
                for (std::size_t i = 0; i < m->triangle_count(); ++i) {
                    const auto [a, b, c] = m->triangle(i);
                    bounds.push_back(triangle_bounds(a, b, c));
                    kinds.push_back(PrimitiveKind::triangle);
                }
                triangles += m->triangle_count();
            }, [&](const Instance& inst) {
                assert(inst.bvh() < bvh_bounds.size());
                // An empty tree has no centroid to bin: whoever plans the levels never places one.
                assert(bvh_bounds[inst.bvh()].x.min <= bvh_bounds[inst.bvh()].x.max);
                bounds.push_back(inst.transform().apply_bounds(bvh_bounds[inst.bvh()]));
                kinds.push_back(PrimitiveKind::instance);
                ++instances;
            },
        }, item);
        // clang-format on
    }
    assert(bounds.size() <= std::numeric_limits<std::uint32_t>::max());

    LevelBuild out;
    BvhBuildResult result = build_bvh_topology(bounds, kinds, settings);
    out.topology = std::move(result.topology);
    out.store.reserve(spheres, quads, triangles);
    out.instances.reserve(instances);

    // Registered in item order, before any triangle lands, so a mesh's index is its rank among the meshes.
    std::vector<std::uint32_t> mesh_entry(items.size());
    for (std::size_t i = 0; i < mesh_entry.size(); ++i) {
        if (const auto* mesh = std::get_if<const Mesh*>(&items[i])) mesh_entry[i] = out.store.add_mesh(**mesh);
    }

    // Gathered in leaf order, each kind into its own arrays. The item holding build id `id` is the last one
    // starting at or before it: an empty mesh starts where the next item does, and loses that tie.
    std::vector<std::uint32_t> kind_slot;
    kind_slot.reserve(bounds.size());
    for (const std::uint32_t id : result.primitive_order) {
        const auto after = std::upper_bound(first.begin(), first.end(), id);
        const std::size_t item = static_cast<std::size_t>(std::distance(first.begin(), after) - 1);
        const std::uint32_t local = id - first[item];

        // clang-format off
        std::visit(Overloaded{
            [&](const Sphere* s) { kind_slot.push_back(out.store.add_sphere(*s)); },
            [&](const Quad* q)   { kind_slot.push_back(out.store.add_quad(*q)); },
            [&](const Mesh*)     { kind_slot.push_back(out.store.add_triangle(mesh_entry[item], local)); },
            [&](const Instance& inst) {
                kind_slot.push_back(static_cast<std::uint32_t>(out.instances.size()));
                out.instances.push_back(inst);
            },
        }, items[item]);
        // clang-format on
    }

    rebase_leaf_offsets(out.topology, kind_slot);
    return out;
}

} // namespace pt
