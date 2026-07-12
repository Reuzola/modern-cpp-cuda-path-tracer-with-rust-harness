#pragma once
#include "hittable.hpp"
#include "vec3.hpp"
#include "interval.hpp"
#include <cmath>

class material;

class sphere : public hittable {
    public:
        sphere(const point3& center, double radius, const material* mat) : center(center), radius(std::fmax(0.0, radius)), mat(mat) {}

        bool hit(const ray& r, const interval& ray_t, hit_record& rec) const override {
            const auto oc = center - r.origin();
            const double a = r.direction().length_squared();
            const double h = dot(oc, r.direction());
            const double c = oc.length_squared() - (radius * radius);

            const double discriminant = h*h - a*c;
            if (discriminant < 0) return false;
            const double sqrtd = std::sqrt(discriminant);

            double root = (h - sqrtd) / a;
            if (!ray_t.surrounds(root)) {
                root = (h + sqrtd) / a;
                if (!ray_t.surrounds(root)) {
                    return false;
                }
            }

            rec.t = root;
            rec.p = r.at(root);
            const vec3 outward_normal = (rec.p - center) / radius;
            rec.set_face_normal(r, outward_normal);
            rec.mat = mat;

            return true;
        }

    private:
        point3 center;
        double radius;
        const material* mat = nullptr;
};