#include "pt/geometry/translate.hpp"
#include "pt/core/hit_record.hpp"
#include "pt/core/hittable.hpp"
#include "pt/math/aabb.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/vec3.hpp"
#include <memory>
#include <utility>

namespace pt {

Translate::Translate(std::shared_ptr<Hittable> object, const Vec3& offset) : object(std::move(object)), offset(offset) {
    bbox = this->object->bounding_box() + offset;
}

Aabb Translate::bounding_box() const { return bbox; }

bool Translate::hit(const Ray& r, const Interval& ray_t, HitRecord& rec) const {
    const Ray offset_r(r.origin() - offset, r.direction(), r.time());

    if (!object->hit(offset_r, ray_t, rec)) return false;

    rec.p += offset;
    return true;
}

} // namespace pt
