#pragma once
#include "pt/geometry/primitive_kind.hpp"
#include "pt/math/aabb.hpp"
#include "pt/math/scalar.hpp"
#include <cstdint>

namespace pt {

// One node of the flat BVH. Nodes are laid out depth first, which places the left
// child directly after its parent, so only the right child needs a stored index.
struct BvhNode {
    Aabb bbox;
    // Right child index when interior. When leaf, its first slot: in the build's
    // order as built, in its kind's arrays once a tree has laid them out.
    std::uint32_t offset{};
    std::uint16_t count{}; // Primitive count; 0 is what marks the node interior
    PrimitiveKind kind{};  // Leaf only: whose array offset indexes. Sits in what was padding

    [[nodiscard]] constexpr bool is_leaf() const noexcept { return count > 0; }
};

// Layout guard: bounds + index data (32B float / 56B double) to preserve hot loop cache efficiency.
static_assert(sizeof(BvhNode) == 6 * sizeof(Float) + 8);

} // namespace pt
