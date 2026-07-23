#pragma once
#include "aabb.hpp"
#include "constants.hpp"
#include "hittable.hpp"
#include "hit_record.hpp"
#include "interval.hpp"
#include "random.hpp"
#include "ray.hpp"
#include "vec3.hpp"
#include <cmath>

class material;

class quad final : public hittable {
    public:
        quad(const point3& Q, const vec3& u, const vec3& v, const material* mat) : Q(Q), u(u), v(v), mat(mat) {
            const vec3 n = cross(u, v);
            normal = unit_vector(n);
            D = dot(normal, Q);
            w = n / dot(n, n);
            area = n.length();

            set_bounding_box();
        }

        [[nodiscard]] bool hit(const ray& r, const interval& ray_t, hit_record& rec) const override {
            const double denom = dot(normal, r.direction());
            if (std::fabs(denom) < 1e-8) return false;

            const double t = (D - dot(normal, r.origin())) / denom;
            if (!ray_t.contains(t)) return false;

            const point3 intersection = r.at(t);
            const vec3 planar_hitpt_vector = intersection - Q;
            const double alpha = dot(w, cross(planar_hitpt_vector, v));
            const double beta = dot(w, cross(u, planar_hitpt_vector));
            if (!is_interior(alpha, beta, rec)) return false;

            rec.t = t;
            rec.p = intersection;
            rec.mat = mat;
            rec.set_face_normal(r, normal);

            return true;
        }

        [[nodiscard]] aabb bounding_box() const override { return bbox; }

        [[nodiscard]] double pdf_value(const point3& origin, const vec3& direction) const override {
            hit_record rec;
            if (!this->hit(ray(origin, direction), interval(0.001, infinity), rec)) return 0.0;

            const double distance_squared = rec.t * rec.t * direction.length_squared();
            const double cosine = std::fabs(dot(direction, rec.normal)) / direction.length();

            return distance_squared / (cosine * area);
        }

        [[nodiscard]] vec3 random(const point3& origin) const override {
            const point3 point = Q + random_double() * u + random_double() * v;
            return point - origin;
        }
    private:
        point3 Q;
        vec3 u, v;
        const material* mat = nullptr;
        aabb bbox;
        vec3 normal;
        double D{};
        vec3 w;
        double area{};

        void set_bounding_box() {
            const auto bbox_diagonal1 = aabb(Q, Q + u + v);
            const auto bbox_diagonal2 = aabb(Q + u, Q + v);

            bbox = aabb(bbox_diagonal1, bbox_diagonal2);
        }

        [[nodiscard]] bool is_interior(double a, double b, hit_record& rec) const {
            static constexpr interval unit_interval{0, 1};
            if (!unit_interval.contains(a) || !unit_interval.contains(b)) return false;

            rec.u = a;
            rec.v = b;
            return true;
        }
};