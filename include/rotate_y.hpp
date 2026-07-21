#pragma once
#include "aabb.hpp"
#include "constants.hpp"
#include "hit_record.hpp"
#include "hittable.hpp"
#include "ray.hpp"
#include "vec3.hpp"
#include <cmath>
#include <memory>

class rotate_y : public hittable {
    public:
        rotate_y(std::shared_ptr<hittable> object, double angle) : object(std::move(object)) {
            const double radians = degrees_to_radians(angle);
            sin_theta = std::sin(radians);
            cos_theta = std::cos(radians);

            const aabb box = this->object->bounding_box();
            point3 min_pt(infinity, infinity, infinity);
            point3 max_pt(-infinity, -infinity, -infinity);

            for (int i = 0; i < 2; i++) {
                for (int j = 0; j < 2; j++) {
                    for (int k = 0; k < 2; k++) {
                        const double x = i ? box.x.max : box.x.min;
                        const double y = j ? box.y.max : box.y.min;
                        const double z = k ? box.z.max : box.z.min;

                        const double newx = cos_theta * x + sin_theta * z;
                        const double newz = -sin_theta * x + cos_theta * z;

                        const vec3 tester(newx, y, newz);
                        for (int c = 0; c < 3; c++) {
                            min_pt[c] = std::fmin(min_pt[c], tester[c]);
                            max_pt[c] = std::fmax(max_pt[c], tester[c]);
                        }
                    }
                }
            }

            bbox = aabb(min_pt, max_pt);
        }

        [[nodiscard]] aabb bounding_box() const override { return bbox; }

        [[nodiscard]] bool hit(const ray& r, const interval& ray_t, hit_record& rec) const override {
            double x_prime = cos_theta * r.origin().x() - sin_theta * r.origin().z();
            double z_prime = sin_theta * r.origin().x() + cos_theta * r.origin().z();
            const point3 origin(x_prime, r.origin().y(), z_prime);

            x_prime = cos_theta * r.direction().x() - sin_theta * r.direction().z();
            z_prime = sin_theta * r.direction().x() + cos_theta * r.direction().z();
            const vec3 direction(x_prime, r.direction().y(), z_prime);

            const ray rotated_r(origin, direction, r.time());
            if (!object->hit(rotated_r, ray_t, rec)) return false;

            x_prime = cos_theta * rec.p.x() + sin_theta * rec.p.z();
            z_prime = -sin_theta * rec.p.x() + cos_theta * rec.p.z();
            const point3 new_p(x_prime, rec.p.y(), z_prime);
            rec.p = new_p;

            x_prime = cos_theta * rec.normal.x() + sin_theta * rec.normal.z();
            z_prime = -sin_theta * rec.normal.x() + cos_theta * rec.normal.z();
            const vec3 new_n(x_prime, rec.normal.y(), z_prime);
            rec.normal = new_n;

            return true;
        }
    private:
        std::shared_ptr<hittable> object;
        double sin_theta{};
        double cos_theta{};
        aabb bbox;
};