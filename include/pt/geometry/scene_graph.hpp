#pragma once
#include "pt/geometry/box.hpp"
#include "pt/geometry/bvh.hpp"
#include "pt/geometry/level.hpp"
#include "pt/geometry/top_level_bvh.hpp"
#include "pt/math/transform.hpp"
#include <memory>
#include <span>
#include <variant>
#include <vector>

namespace pt {

class Mesh;
class Quad;
class Sphere;
struct GroupNode;
struct TransformNode;

// One node of the scene graph as the loader builds it. The graph says only what is where; which trees its
// geometry ends up in is decided when it is flattened. A node may sit under any number of parents.
using SceneNode = std::variant<const Sphere*, const Quad*, BoxFaces, const Mesh*, const GroupNode*, const TransformNode*>;

// Children that share their parent's space. Organisation only: a group leaves no trace in any tree.
struct GroupNode {
    std::vector<SceneNode> children;
};

// A child placed somewhere else. The child is referred to, not copied, so placing one node under several
// transforms instances it.
struct TransformNode {
    Transform transform;
    SceneNode child;
};

// Owns the graph's inner nodes for as long as the graph is needed: until its trees are built.
class SceneGraph {
public:
    [[nodiscard]] const GroupNode* add_group(std::vector<SceneNode> children);
    [[nodiscard]] const TransformNode* add_transform(const Transform& transform, SceneNode child);

private:
    // A node's address is its identity, so growing the graph must never move one. Held by unique_ptr rather
    // than in a deque, whose move may allocate: a scene holding this graph promises a move that cannot throw.
    std::vector<std::unique_ptr<GroupNode>> groups_;
    std::vector<std::unique_ptr<TransformNode>> transforms_;
};

// One level as flattening leaves it, before its tree is built.
struct PlannedLevel {
    std::vector<LevelItem> items;
    bool lone_box{}; // The level is one box and nothing else: built as a single leaf
};

// Every tree a graph needs, before any is built: the bottom levels, and the top level whose instances name
// them by index. A bottom level is planned once per distinct node a transform places, however often it is placed.
struct LevelPlan {
    PlannedLevel top;
    std::vector<PlannedLevel> bottoms;
};

/// Flattens the graph under `roots` into two levels. Groups dissolve into the level they sit in; a transform
/// opens a bottom level for its child's own geometry; a transform under a transform is composed with it and
/// lifted to the top, so no bottom level holds an instance. Order is kept throughout: the graph's order is
/// what breaks ties in each tree's build.
[[nodiscard]] LevelPlan plan_levels(std::span<const SceneNode> roots);

/// Builds every tree `plan` describes, bottom levels first, each with `bottom_settings` unless it is a lone
/// box. Each tree built is folded into `stats` when it is non-null.
[[nodiscard]] TopLevelBvh build_levels(const LevelPlan& plan, BvhStats* stats = nullptr, const BvhBuildSettings& top_settings = {},
                                       const BvhBuildSettings& bottom_settings = {});

} // namespace pt
