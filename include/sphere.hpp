#pragma once
#include "hittable.hpp"
#include "vec3.hpp"
#include <cmath>

class sphere : public hittable {
    private:
        point3 center;
        double radius;
    public:
        sphere(const point3& center, double radius) : center(center), radius(std::fmax(0.0, radius)) {}

        bool hit(const ray& r, double ray_tmin, double ray_tmax, hit_record& rec) const override {
            const auto oc = center - r.origin();
            const double a = r.direction().length_squared();
            const double h = dot(oc, r.direction());
            const double c = oc.length_squared() - (radius * radius);

            const double discriminant = h*h - a*c;
            if (discriminant < 0) return false;
            const double sqrtd = std::sqrt(discriminant);

            double root = (h - sqrtd) / a;
            if (root <= ray_tmin || ray_tmax <= root) {
                root = (h + sqrtd) / a;
                if (root <= ray_tmin || ray_tmax <= root) {
                    return false;
                }
            }

            rec.t = root;
            rec.p = r.at(root);
            rec.normal = (rec.p - center) / radius;
            return true;
        }
};