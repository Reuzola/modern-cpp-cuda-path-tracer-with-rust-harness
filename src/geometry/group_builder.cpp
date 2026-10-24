#include "pt/geometry/group_builder.hpp"
#include "pt/core/hittable.hpp"
#include "pt/geometry/box.hpp"
#include "pt/geometry/bvh.hpp"
#include "pt/geometry/quad.hpp"
#include "pt/geometry/sphere.hpp"
#include "pt/util/arena.hpp"
#include "pt/util/overloaded.hpp"
#include <cassert>
#include <span>
#include <utility>
#include <variant>
#include <vector>

namespace pt {

void GroupBuilder::add(GeometryRef object) {
    // clang-format off
    std::visit(Overloaded{
        [this](const Sphere* sphere)     { assert(sphere != nullptr);   spheres_.push_back(sphere); },
        [this](const Quad* quad)         { assert(quad != nullptr);     quads_.push_back(quad); },
        [this](const BoxFaces& box_faces) {
            for (const Quad* face : box_faces.faces) {
                assert(face != nullptr);
                quads_.push_back(face);
            }
        },
        [this](const Hittable* hittable) { assert(hittable != nullptr); others_.push_back(hittable); },
    }, object);
    // clang-format on
}

const Hittable* GroupBuilder::build(Arena<Hittable>& arena, BvhStats* stats, const BvhBuildSettings& settings) && {
    std::vector<PrimitiveRef> children;

    if (!spheres_.empty()) children.emplace_back(make_bvh(arena, spheres_, stats, settings));
    if (!quads_.empty()) children.emplace_back(make_bvh(arena, quads_, stats, settings));
    children.insert(children.end(), others_.begin(), others_.end());

    if (children.empty()) return arena.create<Bvh>(std::span<const PrimitiveRef>{});
    if (children.size() == 1) return std::get<const Hittable*>(children.front());
    return make_bvh(arena, children, stats, settings);
}

const Hittable* seal(Arena<Hittable>& arena, GeometryRef object, BvhStats* stats) {
    // clang-format off
    return std::visit(Overloaded{
        [&](const BoxFaces& box_faces) -> const Hittable* { return make_box_bvh(arena, box_faces, stats); },
        [&](auto other) -> const Hittable* {
            GroupBuilder builder;
            builder.add(other);
            return std::move(builder).build(arena, stats);
        },
    }, object);
    // clang-format on
}

} // namespace pt
