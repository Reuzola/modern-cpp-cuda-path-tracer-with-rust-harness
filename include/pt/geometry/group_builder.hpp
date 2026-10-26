#pragma once
#include "pt/core/hittable.hpp"
#include "pt/geometry/box.hpp"
#include "pt/geometry/bvh.hpp"
#include "pt/geometry/quad.hpp"
#include "pt/geometry/sphere.hpp"
#include "pt/util/arena.hpp"
#include <variant>
#include <vector>

namespace pt {

// A child as its container receives it, typed by what it becomes in the container's tree: a sphere or a quad
// a primitive of its kind, a box's faces six quads, anything else an opaque child.
// The type is the one the caller holds - a sphere passed as a Hittable is opaque, and is hit through its own virtual call.
using GeometryRef = std::variant<const Sphere*, const Quad*, BoxFaces, const Hittable*>;

// Collects one container's children in the order they come and builds one tree over all of them: a leaf
// holds one kind, the tree every kind the container has. Used once: build() consumes it.
class GroupBuilder {
public:
    void add(GeometryRef object);

    // A lone opaque child is returned as it is: a tree over it would only add a box test, a leaf test and a call.
    // No child at all yields an empty tree, so a scene of media alone has a world.
    [[nodiscard]] const Hittable* build(Arena<Hittable>& arena, BvhStats* stats = nullptr, const BvhBuildSettings& settings = {}) &&;

private:
    std::vector<PrimitiveRef> primitives_; // In the order added; a box's faces in a row
};

// One object as a single Hittable: a sphere or a quad gets a tree of its own, anything else is handed back unchanged.
// For the places that take exactly one child - a transform, a medium's boundary. A box keeps its faces in one leaf.
[[nodiscard]] const Hittable* seal(Arena<Hittable>& arena, GeometryRef object, BvhStats* stats = nullptr);

} // namespace pt
