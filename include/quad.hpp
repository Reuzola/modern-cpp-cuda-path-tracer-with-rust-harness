#pragma once
#include "aabb.hpp"
#include "hittable.hpp"
#include "hit_record.hpp"
#include "interval.hpp"
#include "vec3.hpp"
#include <cmath>

class material;

class quad : public hittable {
    public:
        quad(const point3& Q, const vec3& u, const vec3& v, const material* mat) : Q(Q), u(u), v(v), mat(mat) {
            const vec3 n = cross(u, v);
            normal = unit_vector(n);
            D = dot(normal, Q);
            w = n / dot(n, n);

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
    private:
        point3 Q;
        vec3 u, v;
        const material* mat = nullptr;
        aabb bbox;
        vec3 normal;
        double D{};
        vec3 w;

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