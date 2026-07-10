#pragma once
#include "hit_record.hpp"
#include "hittable.hpp"
#include "ray.hpp"
#include <memory>
#include <vector>

class hittable_list : public hittable {
    private:
        std::vector<std::shared_ptr<hittable>> objects;
    public:
        [[nodiscard]] bool hit(const ray& r, double ray_tmin, double ray_tmax, hit_record& rec) const override {
            double closest_so_far = ray_tmax;
            hit_record temp_rec;

            bool is_hit = false;
            for (const auto &obj : objects) {
                if (obj->hit(r, ray_tmin, closest_so_far, temp_rec)) {
                    closest_so_far = temp_rec.t;
                    rec = temp_rec;
                    is_hit = true;

                }
            }

            return is_hit;
        }

        void clear() {
            objects.clear();
        }

        void add(std::shared_ptr<hittable> obj) {
            objects.push_back(std::move(obj));
        }
};