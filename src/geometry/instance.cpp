#include "pt/geometry/instance.hpp"
#include "pt/core/hit_record.hpp"
#include "pt/core/hittable.hpp"
#include "pt/math/aabb.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/transform.hpp"
#include <cassert>

namespace pt {

Instance::Instance(const Hittable* object, const Transform& transform)
    : object_(object), transform_(transform), bbox_(transform.apply_bounds(object->bounding_box())) {
    assert(object != nullptr);
}

bool Instance::hit(const Ray& r, const Interval& ray_t, HitRecord& rec, Sampler& sampler) const {
    // The direction is deliberately left unnormalised: t then means the same
    // distance in both spaces, so ray_t passes through and rec.t needs no rescaling.
    const Ray local_r(transform_.apply_inverse_point(r.origin()), transform_.apply_inverse_vector(r.direction()), r.time());

    if (!object_->hit(local_r, ray_t, rec, sampler)) return false;

    // front_face is not recomputed: the child's object-space test agrees with the
    // world-space one under any affine transform, mirroring ones included.
    rec.p = transform_.apply_point(rec.p);
    rec.normal = transform_.apply_normal(rec.normal);

    return true;
}

Aabb Instance::bounding_box() const { return bbox_; }

} // namespace pt
