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

// A child as its container receives it, typed by what will accelerate it: a sphere or a quad joins
// its kind's set, anything else stays a child in its own right. A box's faces join the quad set too, as six quads.
// The type is the one the caller holds - a sphere passed as a Hittable is opaque, and is hit through its own virtual call.
using GeometryRef = std::variant<const Sphere*, const Quad*, BoxFaces, const Hittable*>;

// Collects one container's children and builds what they are hit through: one set per primitive
// kind, then a tree over those sets and the opaque children. Used once: build() consumes it.
class GroupBuilder {
public:
    void add(GeometryRef object);

    // A single child is returned as it is: a tree over one child would only add a box test, a leaf
    // test and a call. No child at all yields an empty tree, so a scene of media alone has a world.
    [[nodiscard]] const Hittable* build(Arena<Hittable>& arena, BvhStats* stats = nullptr, const BvhBuildSettings& settings = {}) &&;

private:
    std::vector<PrimitiveRef> spheres_;
    std::vector<PrimitiveRef> quads_;
    std::vector<PrimitiveRef> others_;
};

// One object as a single Hittable: a sphere or a quad gets a set of its own, anything else is handed back unchanged.
// For the places that take exactly one child - a transform, a medium's boundary. A box keeps its faces in one leaf.
[[nodiscard]] const Hittable* seal(Arena<Hittable>& arena, GeometryRef object, BvhStats* stats = nullptr);

} // namespace pt
