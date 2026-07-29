#include "pt/geometry/quad.hpp"
#include "pt/core/hit_record.hpp"
#include "pt/math/aabb.hpp"
#include "pt/math/constants.hpp"
#include "pt/math/interval.hpp"
#include "pt/math/random.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include <cmath>

namespace pt {

quad::quad(const point3& Q, const vec3& u, const vec3& v, const material* mat) : Q(Q), u(u), v(v), mat(mat) {
    const vec3 n = cross(u, v);
    normal = unit_vector(n);
    D = dot(normal, Q);
    w = n / dot(n, n);
    area = n.length();

    set_bounding_box();
}

bool quad::hit(const ray& r, const interval& ray_t, hit_record& rec) const {
    const Float denom = dot(normal, r.direction());
    if (std::fabs(denom) < 1e-8_f) return false;

    const Float t = (D - dot(normal, r.origin())) / denom;
    if (!ray_t.contains(t)) return false;

    const point3 intersection = r.at(t);
    const vec3 planar_hitpt_vector = intersection - Q;
    const Float alpha = dot(w, cross(planar_hitpt_vector, v));
    const Float beta = dot(w, cross(u, planar_hitpt_vector));
    if (!is_interior(alpha, beta, rec)) return false;

    rec.t = t;
    rec.p = intersection;
    rec.mat = mat;
    rec.set_face_normal(r, normal);

    return true;
}

aabb quad::bounding_box() const { return bbox; }

Float quad::pdf_value(const point3& origin, const vec3& direction) const {
    hit_record rec;
    if (!this->hit(ray(origin, direction), interval(0.001_f, infinity), rec)) return 0.0_f;

    const Float distance_squared = rec.t * rec.t * direction.length_squared();
    const Float cosine = std::fabs(dot(direction, rec.normal)) / direction.length();

    return distance_squared / (cosine * area);
}

vec3 quad::random(const point3& origin) const {
    const point3 point = Q + random_scalar() * u + random_scalar() * v;
    return point - origin;
}

void quad::set_bounding_box() {
    const auto bbox_diagonal1 = aabb(Q, Q + u + v);
    const auto bbox_diagonal2 = aabb(Q + u, Q + v);

    bbox = aabb(bbox_diagonal1, bbox_diagonal2);
}

bool quad::is_interior(Float a, Float b, hit_record& rec) const {
    static constexpr interval unit_interval{0, 1};
    if (!unit_interval.contains(a) || !unit_interval.contains(b)) return false;

    rec.u = a;
    rec.v = b;
    return true;
}

} // namespace pt
