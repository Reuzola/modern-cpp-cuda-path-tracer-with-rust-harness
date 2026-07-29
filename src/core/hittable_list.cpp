#include "pt/core/hittable_list.hpp"
#include "pt/core/hit_record.hpp"
#include "pt/core/hittable.hpp"
#include "pt/math/aabb.hpp"
#include "pt/math/interval.hpp"
#include "pt/math/random.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include <cstddef>
#include <memory>
#include <utility>

namespace pt {

HittableList::HittableList(std::shared_ptr<Hittable> object) { add(std::move(object)); }

Aabb HittableList::bounding_box() const { return bbox; }

bool HittableList::hit(const Ray& r, const Interval& ray_t, HitRecord& rec) const {
    Float closest_so_far = ray_t.max;
    HitRecord temp_rec;

    bool is_hit = false;
    for (const auto& obj : objects) {
        if (obj->hit(r, Interval(ray_t.min, closest_so_far), temp_rec)) {
            closest_so_far = temp_rec.t;
            rec = temp_rec;
            is_hit = true;
        }
    }
    return is_hit;
}

Float HittableList::pdf_value(const Point3& origin, const Vec3& direction) const {
    if (objects.empty()) return 0.0_f;

    const Float weight = 1.0_f / static_cast<Float>(objects.size());
    Float sum{0.0_f};

    for (const auto& object : objects) {
        sum += weight * object->pdf_value(origin, direction);
    }
    return sum;
}

Vec3 HittableList::random(const Point3& origin) const {
    if (objects.empty()) return Vec3(0, 0, 0);

    const int count = static_cast<int>(objects.size());
    return objects[static_cast<std::size_t>(random_int(0, count - 1))]->random(origin);
}

void HittableList::clear() {
    objects.clear();
    bbox = Aabb();
}

void HittableList::add(std::shared_ptr<Hittable> obj) {
    bbox = Aabb(bbox, obj->bounding_box());
    objects.push_back(std::move(obj));
}

} // namespace pt
