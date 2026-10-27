#include "pt/geometry/transformed_hittable.hpp"
#include "pt/core/hit_record.hpp"
#include "pt/core/hittable.hpp"
#include "pt/math/aabb.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/robust.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/transform.hpp"
#include <cassert>

namespace pt {

namespace {

// One component of apply_point is a three-term dot product plus the translation: gamma(4)
// relative to the terms' magnitudes. Randomized tests needed gamma(2), so it is also twice that.
constexpr int transform_rounding_order = 4;

} // namespace

TransformedHittable::TransformedHittable(const Hittable* object, const Transform& transform)
    : object_(object), transform_(transform),
      bbox_(transform.apply_bounds(object->bounding_box())),
      margin_scale_(transform.stretch_bound()) {
    assert(object != nullptr);
}

bool TransformedHittable::hit(const Ray& r, const Interval& ray_t, HitRecord& rec) const {
    // The direction is deliberately left unnormalised: t then means the same
    // distance in both spaces, so ray_t passes through and rec.t needs no rescaling.
    const Ray local_r(transform_.apply_inverse_point(r.origin()), transform_.apply_inverse_vector(r.direction()), r.time());

    if (!object_->hit(local_r, ray_t, rec)) return false;

    // The transform rounds terms the size of |M||p| + |t|. Where they cancel to a point near the
    // world origin that error outgrows the ulp offset, so it joins the margin.
    const Float rounding = gamma(transform_rounding_order) * transform_.apply_point_magnitude(rec.p);

    // front_face is not recomputed: the child's object-space test agrees with the
    // world-space one under any affine transform, mirroring ones included.
    rec.p = transform_.apply_point(rec.p);
    rec.normal = transform_.apply_normal(rec.normal);
    rec.geometric_normal = transform_.apply_normal(rec.geometric_normal);
    rec.spawn_margin = margin_scale_ * rec.spawn_margin + rounding;

    return true;
}

Aabb TransformedHittable::bounding_box() const { return bbox_; }

} // namespace pt
