#pragma once
#include "constants.hpp"
#include "hittable.hpp"
#include "ray.hpp"
#include "vec3.hpp"
#include "interval.hpp"
#include "aabb.hpp"
#include <cmath>

class material;

class sphere : public hittable {
    public:
        sphere(const point3& center1, const point3& center2, double radius, const material* mat) : center(center1, center2 - center1), radius(std::fmax(0.0, radius)), mat(mat) {
            const auto rvec = vec3(this->radius, this->radius, this->radius);
            const aabb box1(center.at(0) - rvec, center.at(0) + rvec);
            const aabb box2(center.at(1) - rvec, center.at(1) + rvec);
            bbox = aabb(box1, box2);
        }
        sphere(const point3& static_center, double radius, const material* mat) : sphere(static_center, static_center, radius, mat) {}

        [[nodiscard]] aabb bounding_box() const override { return bbox; }

        bool hit(const ray& r, const interval& ray_t, hit_record& rec) const override {
            const auto current_center = center.at(r.time());

            const auto oc = current_center - r.origin();
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
            const vec3 outward_normal = (rec.p - current_center) / radius;
            rec.set_face_normal(r, outward_normal);
            const auto [u, v] = get_sphere_uv(outward_normal);
            rec.u = u;
            rec.v = v;
            rec.mat = mat;

            return true;
        }
    private:
        ray center;
        double radius;
        const material* mat = nullptr;
        aabb bbox;

        [[nodiscard]] static uv_coords get_sphere_uv(const point3& p) {
            const double theta = std::acos(-p.y());
            const double phi = std::atan2(-p.z(), p.x()) + pi;

            const double u = phi / (2 * pi);
            const double v = theta / pi;
            return {u, v};
        }
};