#include "pt/geometry/quad.hpp"
#include "pt/core/hit_record.hpp"
#include "pt/math/aabb.hpp"
#include "pt/math/constants.hpp"
#include "pt/math/interval.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/sampler.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include <cmath>

namespace pt {

Quad::Quad(const Point3& Q, const Vec3& u, const Vec3& v, const Material* mat) : Q_(Q), u_(u), v_(v), mat_(mat) {
    const Vec3 n = cross(u, v);
    normal_ = unit_vector(n);
    D_ = dot(normal_, Q);
    w_ = n / dot(n, n);
    area_ = n.length();

    set_bounding_box();
}

bool Quad::hit(const Ray& r, const Interval& ray_t, HitRecord& rec, Sampler&) const {
    return intersect(r, ray_t, rec);
}

Aabb Quad::bounding_box() const { return bbox_; }

Float Quad::pdf_direction(const Point3& origin, const Vec3& direction) const {
    HitRecord rec;
    if (!intersect(Ray(origin, direction), Interval(0.001_f, infinity), rec)) return 0.0_f;

    const Float distance_squared = rec.t * rec.t * direction.length_squared();
    const Float cosine = std::fabs(dot(direction, rec.normal)) / direction.length();

    return distance_squared / (cosine * area_);
}

Vec3 Quad::sample_direction(const Point3& origin, Sampler& sampler) const {
    const Float a = sampler.next_scalar();
    const Float b = sampler.next_scalar();
    const Point3 point = Q_ + a * u_ + b * v_;
    return point - origin;
}

void Quad::set_bounding_box() {
    const auto bbox_diagonal1 = Aabb(Q_, Q_ + u_ + v_);
    const auto bbox_diagonal2 = Aabb(Q_ + u_, Q_ + v_);

    bbox_ = Aabb(bbox_diagonal1, bbox_diagonal2);
}

bool Quad::is_interior(Float a, Float b, HitRecord& rec) const {
    static constexpr Interval unit_interval{0, 1};
    if (!unit_interval.contains(a) || !unit_interval.contains(b)) return false;

    rec.u = a;
    rec.v = b;
    return true;
}

bool Quad::intersect(const Ray& r, const Interval& ray_t, HitRecord& rec) const {
    const Float denom = dot(normal_, r.direction());
    if (std::fabs(denom) < 1e-8_f) return false;

    const Float t = (D_ - dot(normal_, r.origin())) / denom;
    if (!ray_t.contains(t)) return false;

    const Point3 intersection = r.at(t);
    const Vec3 planar_hitpt_vector = intersection - Q_;
    const Float alpha = dot(w_, cross(planar_hitpt_vector, v_));
    const Float beta = dot(w_, cross(u_, planar_hitpt_vector));
    if (!is_interior(alpha, beta, rec)) return false;

    rec.t = t;
    rec.p = intersection;
    rec.mat = mat_;
    rec.set_face_normal(r, normal_);

    return true;
}

} // namespace pt
