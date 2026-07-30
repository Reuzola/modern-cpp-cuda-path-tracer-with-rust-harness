#include "pt/geometry/translate.hpp"
#include "pt/core/hit_record.hpp"
#include "pt/core/hittable.hpp"
#include "pt/math/aabb.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/vec3.hpp"

namespace pt {

Translate::Translate(const Hittable* object, const Vec3& offset) : object_(object), offset_(offset) {
    bbox_ = object_->bounding_box() + offset;
}

Aabb Translate::bounding_box() const { return bbox_; }

bool Translate::hit(const Ray& r, const Interval& ray_t, HitRecord& rec) const {
    const Ray offset_r(r.origin() - offset_, r.direction(), r.time());

    if (!object_->hit(offset_r, ray_t, rec)) return false;

    rec.p += offset_;
    return true;
}

} // namespace pt
