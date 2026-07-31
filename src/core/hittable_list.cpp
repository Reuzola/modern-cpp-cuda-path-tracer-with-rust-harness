#include "pt/core/hittable_list.hpp"
#include "pt/core/hit_record.hpp"
#include "pt/core/hittable.hpp"
#include "pt/math/aabb.hpp"
#include "pt/math/interval.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/sampler.hpp"
#include "pt/math/scalar.hpp"
#include <cassert>

namespace pt {

HittableList::HittableList(const Hittable* object) { add(object); }

Aabb HittableList::bounding_box() const { return bbox_; }

bool HittableList::hit(const Ray& r, const Interval& ray_t, HitRecord& rec, Sampler& sampler) const {
    Float closest_so_far = ray_t.max;
    HitRecord temp_rec;

    bool is_hit = false;
    for (const Hittable* obj : objects_) {
        if (obj->hit(r, Interval(ray_t.min, closest_so_far), temp_rec, sampler)) {
            closest_so_far = temp_rec.t;
            rec = temp_rec;
            is_hit = true;
        }
    }
    return is_hit;
}

void HittableList::clear() {
    objects_.clear();
    bbox_ = Aabb();
}

void HittableList::add(const Hittable* obj) {
    assert(obj != nullptr);

    bbox_ = Aabb(bbox_, obj->bounding_box());
    objects_.push_back(obj);
}

} // namespace pt
