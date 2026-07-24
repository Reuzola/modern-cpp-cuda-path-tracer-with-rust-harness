#pragma once
#include "aabb.hpp"
#include "hit_record.hpp"
#include "hittable.hpp"
#include "interval.hpp"
#include "random.hpp"
#include "ray.hpp"
#include "vec3.hpp"
#include <memory>
#include <vector>

class hittable_list final : public hittable {
        friend class bvh_node;
    public:
        hittable_list() = default;
        explicit hittable_list(std::shared_ptr<hittable> object) { add(std::move(object)); }

        [[nodiscard]] aabb bounding_box() const override { return bbox; }

        [[nodiscard]] bool hit(const ray& r, const interval& ray_t, hit_record& rec) const override {
            double closest_so_far = ray_t.max;
            hit_record temp_rec;

            bool is_hit = false;
            for (const auto &obj : objects) {
                if (obj->hit(r, interval(ray_t.min, closest_so_far), temp_rec)) {
                    closest_so_far = temp_rec.t;
                    rec = temp_rec;
                    is_hit = true;

                }
            }
            return is_hit;
        }

        [[nodiscard]] double pdf_value(const point3& origin, const vec3& direction) const override {
            if (objects.empty()) return 0.0;

            const double weight = 1.0 / objects.size();
            double sum{0.0};

            for (const auto& object : objects) {
                sum += weight * object->pdf_value(origin, direction);
            }
            return sum;
        }

        [[nodiscard]] vec3 random(const point3& origin) const override {
            if (objects.empty()) return vec3(0, 0, 0);
            
            const int count = static_cast<int>(objects.size());
            return objects[random_int(0, count - 1)]->random(origin);
        }

        void clear() {
            objects.clear();
            bbox = aabb();
        }

        void add(std::shared_ptr<hittable> obj) {
            bbox = aabb(bbox, obj->bounding_box());
            objects.push_back(std::move(obj));
        }
    private:
        std::vector<std::shared_ptr<hittable>> objects;
        aabb bbox;
};