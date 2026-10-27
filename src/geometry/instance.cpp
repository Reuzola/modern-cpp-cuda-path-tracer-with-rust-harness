#include "pt/geometry/instance.hpp"
#include "pt/core/hit_record.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/robust.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/transform.hpp"
#include <cstdint>

namespace pt {

namespace {

// One component of apply_point is a three-term dot product plus the translation: gamma(4)
// relative to the terms' magnitudes. Randomized tests needed gamma(2), so it is also twice that.
constexpr int transform_rounding_order = 4;

} // namespace

Instance::Instance(const Transform& transform, std::uint32_t bvh) noexcept
    : transform_(transform), bvh_(bvh), margin_scale_(transform.stretch_bound()) {}

Ray Instance::to_local(const Ray& r) const noexcept {
    return Ray(transform_.apply_inverse_point(r.origin()), transform_.apply_inverse_vector(r.direction()), r.time());
}

void Instance::to_world(HitRecord& rec) const noexcept {
    // The transform rounds terms the size of |M||p| + |t|. Where they cancel to a point near the
    // world origin that error outgrows the ulp offset, so it joins the margin.
    const Float rounding = gamma(transform_rounding_order) * transform_.apply_point_magnitude(rec.p);

    // front_face is not recomputed: the tree's object-space test agrees with the
    // world-space one under any affine transform, mirroring ones included.
    rec.p = transform_.apply_point(rec.p);
    rec.normal = transform_.apply_normal(rec.normal);
    rec.geometric_normal = transform_.apply_normal(rec.geometric_normal);
    rec.spawn_margin = margin_scale_ * rec.spawn_margin + rounding;
}

} // namespace pt
