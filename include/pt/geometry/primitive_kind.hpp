#pragma once
#include <cstdint>

namespace pt {

// What a BVH leaf holds. A leaf never mixes kinds, so testing it is one dispatch
// to one kind's kernel, never a dispatch per primitive.
enum class PrimitiveKind : std::uint8_t {
    sphere,
    quad,
    triangle,
    opaque, // A child hit through its own virtual hit(): a mesh's tree, an instance or a nested group.
};

// One byte: BvhNode keeps it in what was padding.
static_assert(sizeof(PrimitiveKind) == 1);

} // namespace pt
