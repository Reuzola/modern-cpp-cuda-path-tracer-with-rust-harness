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

Quad::Quad(const Point3& Q, const Vec3& u, const Vec3& v, const Material* mat) : Q(Q), u(u), v(v), mat(mat) {
    const Vec3 n = cross(u, v);
    normal = unit_vector(n);
    D = dot(normal, Q);
    w = n / dot(n, n);
    area = n.length();

    set_bounding_box();
}

bool Quad::hit(const Ray& r, const Interval& ray_t, HitRecord& rec) const {
    const Float denom = dot(normal, r.direction());
    if (std::fabs(denom) < 1e-8_f) return false;

    const Float t = (D - dot(normal, r.origin())) / denom;
    if (!ray_t.contains(t)) return false;

    const Point3 intersection = r.at(t);
    const Vec3 planar_hitpt_vector = intersection - Q;
    const Float alpha = dot(w, cross(planar_hitpt_vector, v));
    const Float beta = dot(w, cross(u, planar_hitpt_vector));
    if (!is_interior(alpha, beta, rec)) return false;

    rec.t = t;
    rec.p = intersection;
    rec.mat = mat;
    rec.set_face_normal(r, normal);

    return true;
}

Aabb Quad::bounding_box() const { return bbox; }

Float Quad::pdf_value(const Point3& origin, const Vec3& direction) const {
    HitRecord rec;
    if (!this->hit(Ray(origin, direction), Interval(0.001_f, infinity), rec)) return 0.0_f;

    const Float distance_squared = rec.t * rec.t * direction.length_squared();
    const Float cosine = std::fabs(dot(direction, rec.normal)) / direction.length();

    return distance_squared / (cosine * area);
}

Vec3 Quad::random(const Point3& origin) const {
    const Point3 point = Q + random_scalar() * u + random_scalar() * v;
    return point - origin;
}

void Quad::set_bounding_box() {
    const auto bbox_diagonal1 = Aabb(Q, Q + u + v);
    const auto bbox_diagonal2 = Aabb(Q + u, Q + v);

    bbox = Aabb(bbox_diagonal1, bbox_diagonal2);
}

bool Quad::is_interior(Float a, Float b, HitRecord& rec) const {
    static constexpr Interval unit_interval{0, 1};
    if (!unit_interval.contains(a) || !unit_interval.contains(b)) return false;

    rec.u = a;
    rec.v = b;
    return true;
}

} // namespace pt
