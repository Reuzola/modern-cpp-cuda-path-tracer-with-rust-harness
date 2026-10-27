#pragma once
#include "pt/core/hittable.hpp"
#include "pt/geometry/bvh.hpp"
#include "pt/geometry/quad.hpp"
#include "pt/math/vec3.hpp"
#include "pt/util/arena.hpp"
#include <array>

namespace pt {

class Material;

// A box's six faces, which live in the arena so a container can take them into its own quad set.
// A value that names the faces; it owns none of them.
struct BoxFaces {
    std::array<const Quad*, 6> faces{};
};

// How a level holding nothing but one box is built: one leaf, the faces tested in a row, as a box always was.
// A depth limit of one spends the root's budget at once, so the builder stops there without consulting the
// SAH. Whether six faces deserve a tree is the cost sweep's question, not this file's.
inline constexpr BvhBuildSettings box_build_settings{.max_leaf_size = 6, .depth_limit = 1};

// An axis-aligned box as six quads. The corners may come in either order.
[[nodiscard]] BoxFaces box(Arena<Hittable>& arena, const Point3& a, const Point3& b, const Material* mat);

// A box on its own - under a transform, or bounding a medium - as a set of its own.
[[nodiscard]] const Bvh* make_box_bvh(Arena<Hittable>& arena, const BoxFaces& box, BvhStats* stats = nullptr);

} // namespace pt
