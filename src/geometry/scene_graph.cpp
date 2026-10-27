#include "pt/geometry/scene_graph.hpp"
#include "pt/geometry/bottom_level_bvh.hpp"
#include "pt/geometry/box.hpp"
#include "pt/geometry/bvh.hpp"
#include "pt/geometry/instance.hpp"
#include "pt/geometry/level.hpp"
#include "pt/geometry/mesh.hpp"
#include "pt/geometry/top_level_bvh.hpp"
#include "pt/math/transform.hpp"
#include "pt/util/overloaded.hpp"
#include <chrono>
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <utility>
#include <variant>
#include <vector>

namespace pt {

namespace {

// A node's identity, whatever it is. A box is its first face: the six are created together and never shared.
[[nodiscard]] const void* identity_of(const SceneNode& node) {
    // clang-format off
    return std::visit(Overloaded{
        [](const BoxFaces& box) -> const void* { return box.faces.front(); },
        [](const auto* pointer) -> const void* { return pointer; },
    }, node);
    // clang-format on
}

// A subtree flattened as a level of its own: the bottom level its own geometry became, if it had any, and the
// instances below it, already composed into its space and waiting for the transform that placed it.
struct Subtree {
    std::optional<std::uint32_t> bottom;
    std::vector<Instance> lifted;
};

class Flattener {
public:
    // Appends `node`'s geometry to a level being collected, in graph order: primitives as themselves,
    // anything under a transform as instances in the level's own space.
    void collect(const SceneNode& node, std::vector<LevelItem>& items) {
        // clang-format off
        std::visit(Overloaded{
            [&](const Sphere* sphere) { items.emplace_back(sphere); },
            [&](const Quad* quad)     { items.emplace_back(quad); },
            [&](const BoxFaces& box) {
                for (const Quad* face : box.faces) {
                    items.emplace_back(face);
                }
            },
            // An empty mesh adds nothing, so a level of empty meshes stays empty and never becomes a tree.
            [&](const Mesh* mesh) {
                if (mesh->triangle_count() > 0) items.emplace_back(mesh);
            },
            [&](const GroupNode* group) {
                for (const SceneNode& child : group->children)
                    collect(child, items);
            },
            [&](const TransformNode* transform) {
                const Subtree& placed = subtree(transform->child);
                if (placed.bottom) items.emplace_back(Instance(transform->transform, *placed.bottom));
                for (const Instance& below : placed.lifted)
                    items.emplace_back(Instance(transform->transform * below.transform(), below.bvh()));
            },
        }, node);
        // clang-format on
    }

    [[nodiscard]] LevelPlan finish(PlannedLevel top) && {
        plan_.top = std::move(top);
        return std::move(plan_);
    }

private:
    LevelPlan plan_;
    std::map<const void*, Subtree> memo_;

    // Flattened once per node, however many transforms place it: its bottom level is shared by them all.
    const Subtree& subtree(const SceneNode& node) {
        const void* key = identity_of(node);
        if (const auto it = memo_.find(key); it != memo_.end()) return it->second;

        std::vector<LevelItem> items;
        collect(node, items);

        Subtree result;
        PlannedLevel own{.items = {}, .lone_box = std::holds_alternative<BoxFaces>(node)};

        // Instances cannot stay in a bottom level: they go up, to be composed with whatever places this one.
        for (const LevelItem& item : items) {
            if (const Instance* instance = std::get_if<Instance>(&item)) {
                result.lifted.push_back(*instance);
            } else {
                own.items.push_back(item);
            }
        }

        if (!own.items.empty()) {
            result.bottom = static_cast<std::uint32_t>(plan_.bottoms.size());
            plan_.bottoms.push_back(std::move(own));
        }

        // std::map keeps the reference valid as later subtrees are added.
        return memo_.emplace(key, std::move(result)).first->second;
    }
};

// Timed around the construction: bounds, build and layout are all part of what the scene pays.
template <typename Tree, typename... Args>
[[nodiscard]] Tree build_timed(BvhStats* stats, Args&&... args) {
    const auto start = std::chrono::steady_clock::now();
    Tree tree(std::forward<Args>(args)...);
    const auto end = std::chrono::steady_clock::now();

    // One tree per bottom level and one for the top: the totals are over those, not over placements.
    if (stats != nullptr && !tree.topology().nodes.empty()) stats->record(tree.topology(), std::chrono::duration_cast<std::chrono::nanoseconds>(end - start));
    return tree;
}

} // namespace

const GroupNode* SceneGraph::add_group(std::vector<SceneNode> children) {
    return &groups_.emplace_back(GroupNode{.children = std::move(children)});
}

const TransformNode* SceneGraph::add_transform(const Transform& transform, SceneNode child) {
    return &transforms_.emplace_back(TransformNode{.transform = transform, .child = child});
}

LevelPlan plan_levels(std::span<const SceneNode> roots) {
    Flattener flattener;
    PlannedLevel top{.items = {}, .lone_box = roots.size() == 1 && std::holds_alternative<BoxFaces>(roots.front())};

    for (const SceneNode& root : roots) {
        flattener.collect(root, top.items);
    }
    return std::move(flattener).finish(std::move(top));
}

TopLevelBvh build_levels(const LevelPlan& plan, BvhStats* stats, const BvhBuildSettings& top_settings, const BvhBuildSettings& bottom_settings) {
    std::vector<BottomLevelBvh> bottoms;
    bottoms.reserve(plan.bottoms.size());
    for (const PlannedLevel& level : plan.bottoms) {
        const BvhBuildSettings& settings = level.lone_box ? box_build_settings : bottom_settings;
        bottoms.push_back(build_timed<BottomLevelBvh>(stats, std::span<const LevelItem>(level.items), settings));
    }

    const BvhBuildSettings& settings = plan.top.lone_box ? box_build_settings : top_settings;
    return build_timed<TopLevelBvh>(stats, std::span<const LevelItem>(plan.top.items), std::move(bottoms), settings);
}

} // namespace pt
