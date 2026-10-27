#pragma once
#include "pt/geometry/bvh.hpp"
#include "pt/geometry/instance.hpp"
#include "pt/geometry/primitive_store.hpp"
#include "pt/math/aabb.hpp"
#include <span>
#include <variant>
#include <vector>

namespace pt {

class Mesh;
class Quad;
class Sphere;

// One entry of a level - the primitives that share one coordinate space - in the order the scene graph gave
// it. A mesh stands for all of its triangles. Only a top level holds instances.
using LevelItem = std::variant<const Sphere*, const Quad*, const Mesh*, Instance>;

// A level's tree with its primitives laid out in leaf order: what both tree types are made of.
struct LevelBuild {
    BvhTopology topology;
    PrimitiveStore store;
    std::vector<Instance> instances; // Leaf order; empty for a bottom level
};

/// Builds one tree over `items`. An instance is bounded by its tree's box carried through its transform;
/// `bvh_bounds[i]` is the box of the tree an instance names by i, and is empty when no item is an instance.
/// Sphere, quad and mesh objects must outlive the result: the store reads a mesh's attributes back.
[[nodiscard]] LevelBuild build_level(std::span<const LevelItem> items, std::span<const Aabb> bvh_bounds, const BvhBuildSettings& settings = {});

} // namespace pt
