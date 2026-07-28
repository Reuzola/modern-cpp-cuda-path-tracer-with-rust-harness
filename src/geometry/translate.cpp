#include "pt/geometry/translate.hpp"
#include "pt/core/hit_record.hpp"
#include "pt/core/hittable.hpp"
#include "pt/math/aabb.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/vec3.hpp"
#include <memory>

namespace pt {

translate::translate(std::shared_ptr<hittable> object, const vec3& offset) : object(std::move(object)), offset(offset) {
    bbox = this->object->bounding_box() + offset;
}

aabb translate::bounding_box() const { return bbox; }

bool translate::hit(const ray& r, const interval& ray_t, hit_record& rec) const {
    const ray offset_r(r.origin() - offset, r.direction(), r.time());

    if (!object->hit(offset_r, ray_t, rec)) return false;

    rec.p += offset;
    return true;
}

} // namespace pt
