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
        [this](const Sphere* sphere)      { assert(sphere != nullptr); primitives_.emplace_back(sphere); },
        [this](const Quad* quad)          { assert(quad != nullptr);   primitives_.emplace_back(quad); },
        [this](const BoxFaces& box_faces) {
            for (const Quad* face : box_faces.faces) {
                assert(face != nullptr);
                primitives_.emplace_back(face);
            }
        },
        [this](const Hittable* hittable) { assert(hittable != nullptr); primitives_.emplace_back(hittable); },
    }, object);
    // clang-format on
}

const Hittable* GroupBuilder::build(Arena<Hittable>& arena, BvhStats* stats, const BvhBuildSettings& settings) && {
    if (primitives_.empty()) return arena.create<Bvh>(std::span<const PrimitiveRef>{});

    // A lone opaque child needs no tree around it. A lone sphere or quad does: only a tree holds its copied shape.
    if (primitives_.size() == 1) {
        if (const auto* opaque = std::get_if<const Hittable*>(&primitives_.front())) return *opaque;
    }

    return make_bvh(arena, primitives_, stats, settings);
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
