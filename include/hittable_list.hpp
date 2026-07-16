#pragma once
#include "aabb.hpp"
#include "hit_record.hpp"
#include "hittable.hpp"
#include "interval.hpp"
#include "ray.hpp"
#include <memory>
#include <vector>

class hittable_list : public hittable {
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