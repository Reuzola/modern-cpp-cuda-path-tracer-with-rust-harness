#include "pt/geometry/group_builder.hpp"
#include "pt/core/hittable.hpp"
#include "pt/core/hittable_list.hpp"
#include "pt/geometry/bvh.hpp"
#include "pt/geometry/quad.hpp"
#include "pt/geometry/shape_bvh.hpp"
#include "pt/geometry/sphere.hpp"
#include "pt/util/arena.hpp"
#include "pt/util/overloaded.hpp"
#include <cassert>
#include <utility>
#include <variant>
#include <vector>

namespace pt {

void GroupBuilder::add(GeometryRef object) {
    // clang-format off
    std::visit(Overloaded{
        [this](const Sphere* sphere)     { assert(sphere != nullptr);   spheres_.push_back(sphere); },
        [this](const Quad* quad)         { assert(quad != nullptr);     quads_.push_back(quad); },
        [this](const Hittable* hittable) { assert(hittable != nullptr); others_.push_back(hittable); },
    }, object);
    // clang-format on
}

const Hittable* GroupBuilder::build(Arena<Hittable>& arena, BvhStats* stats, const BvhBuildSettings& settings) && {
    std::vector<const Hittable*> children;

    // Sets first, then the opaque children in the order they came.
    if (!spheres_.empty()) children.push_back(make_shape_bvh<Sphere>(arena, spheres_, stats, settings));
    if (!quads_.empty()) children.push_back(make_shape_bvh<Quad>(arena, quads_, stats, settings));
    children.insert(children.end(), others_.begin(), others_.end());

    if (children.empty()) return arena.create<HittableList>();
    if (children.size() == 1) return children.front();

    HittableList list;
    for (const Hittable* child : children) {
        list.add(child);
    }

    return make_bvh(arena, list, stats, settings);
}

const Hittable* seal(Arena<Hittable>& arena, GeometryRef object, BvhStats* stats) {
    GroupBuilder builder;
    builder.add(object);
    return std::move(builder).build(arena, stats);
}

} // namespace pt
