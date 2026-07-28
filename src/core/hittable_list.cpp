#include "pt/core/hittable_list.hpp"
#include "pt/core/hit_record.hpp"
#include "pt/core/hittable.hpp"
#include "pt/math/aabb.hpp"
#include "pt/math/interval.hpp"
#include "pt/math/random.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/vec3.hpp"
#include <cstddef>
#include <memory>
#include <utility>

namespace pt {

hittable_list::hittable_list(std::shared_ptr<hittable> object) { add(std::move(object)); }

aabb hittable_list::bounding_box() const { return bbox; }

bool hittable_list::hit(const ray& r, const interval& ray_t, hit_record& rec) const {
    double closest_so_far = ray_t.max;
    hit_record temp_rec;

    bool is_hit = false;
    for (const auto& obj : objects) {
        if (obj->hit(r, interval(ray_t.min, closest_so_far), temp_rec)) {
            closest_so_far = temp_rec.t;
            rec = temp_rec;
            is_hit = true;
        }
    }
    return is_hit;
}

double hittable_list::pdf_value(const point3& origin, const vec3& direction) const {
    if (objects.empty()) return 0.0;

    const double weight = 1.0 / static_cast<double>(objects.size());
    double sum{0.0};

    for (const auto& object : objects) {
        sum += weight * object->pdf_value(origin, direction);
    }
    return sum;
}

vec3 hittable_list::random(const point3& origin) const {
    if (objects.empty()) return vec3(0, 0, 0);

    const int count = static_cast<int>(objects.size());
    return objects[static_cast<std::size_t>(random_int(0, count - 1))]->random(origin);
}

void hittable_list::clear() {
    objects.clear();
    bbox = aabb();
}

void hittable_list::add(std::shared_ptr<hittable> obj) {
    bbox = aabb(bbox, obj->bounding_box());
    objects.push_back(std::move(obj));
}

} // namespace pt
