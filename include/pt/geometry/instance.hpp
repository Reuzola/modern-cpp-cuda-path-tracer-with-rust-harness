#pragma once
#include "pt/core/hit_record.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/transform.hpp"
#include <cstdint>
#include <type_traits>

namespace pt {

// One placement of a bottom-level tree: where it sits, and which tree it is. Trivially copyable and free of
// pointers, so a top-level tree holds its instances by value and a device buffer can hold them as they are.
class Instance {
public:
    // `bvh` indexes the bottom-level trees of the top level this instance is placed in.
    Instance(const Transform& transform, std::uint32_t bvh) noexcept;

    [[nodiscard]] const Transform& transform() const noexcept { return transform_; }
    [[nodiscard]] std::uint32_t bvh() const noexcept { return bvh_; }

    // The ray in the tree's own space. The direction is deliberately left unnormalised: t then means
    // the same distance in both spaces, so an interval passes through and a hit's t needs no rescaling.
    [[nodiscard]] Ray to_local(const Ray& r) const noexcept;

    // Carries a record the tree filled in its own space out to the world. Once per query, for the winner.
    void to_world(HitRecord& rec) const noexcept;

private:
    Transform transform_;
    std::uint32_t bvh_{};
    Float margin_scale_{}; // The tree's margin is an object-space distance; this keeps it a bound in world space.
};

static_assert(std::is_trivially_copyable_v<Instance>);

} // namespace pt
