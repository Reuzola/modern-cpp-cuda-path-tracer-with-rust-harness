#pragma once
#include <cstdint>

namespace pt {

// What a BVH leaf holds. A leaf never mixes kinds, so testing it is one dispatch
// to one kind's kernel, never a dispatch per primitive.
enum class PrimitiveKind : std::uint8_t {
    sphere,
    quad,
    triangle,
    instance, // A placement of a bottom-level tree, entered by moving the ray into its space.
};

// One byte: BvhNode keeps it in what was padding.
static_assert(sizeof(PrimitiveKind) == 1);

// Whether a leaf of this kind holds primitives a store tests, rather than children its tree reaches through.
[[nodiscard]] constexpr bool is_primitive_kind(PrimitiveKind kind) noexcept {
    if (kind == PrimitiveKind::sphere || kind == PrimitiveKind::quad || kind == PrimitiveKind::triangle) return true;
    return false;
}

} // namespace pt
