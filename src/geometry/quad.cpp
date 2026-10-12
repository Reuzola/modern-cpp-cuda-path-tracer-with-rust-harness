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

Quad::Quad(const Point3& q, const Vec3& u, const Vec3& v, const Material* mat) : q_(q), u_(u), v_(v), mat_(mat) {
    const Vec3 n = cross(u, v);
    normal_ = unit_vector(n);
    d_ = dot(normal_, q);
    w_ = n / dot(n, n);
    area_ = n.length();

    set_bounding_box();
}

bool Quad::hit(const Ray& r, const Interval& ray_t, HitRecord& rec) const {
    QuadHit quad_hit;
    if (!find_hit(r, ray_t, quad_hit)) return false;
    fill_record(r, quad_hit, rec);
    return true;
}

Aabb Quad::bounding_box() const { return bbox_; }

Float Quad::pdf_direction(const Point3& origin, const Vec3& direction) const {
    QuadHit quad_hit;
    if (!find_hit(Ray(origin, direction), Interval(0.0_f, infinity), quad_hit)) return 0.0_f;

    const Float distance_squared = quad_hit.t * quad_hit.t * direction.length_squared();
    const Float cosine = std::fabs(dot(direction, normal_)) / direction.length();

    return distance_squared / (cosine * area_);
}

Vec3 Quad::sample_direction(const Point3& origin, Sampler& sampler) const {
    const Float a = sampler.next_scalar();
    const Float b = sampler.next_scalar();
    const Point3 point = q_ + a * u_ + b * v_;
    return point - origin;
}

void Quad::set_bounding_box() {
    const auto bbox_diagonal1 = Aabb(q_, q_ + u_ + v_);
    const auto bbox_diagonal2 = Aabb(q_ + u_, q_ + v_);

    bbox_ = Aabb(bbox_diagonal1, bbox_diagonal2);
}

bool Quad::is_interior(Float a, Float b) noexcept {
    static constexpr Interval unit_interval{0, 1};
    return unit_interval.contains(a) && unit_interval.contains(b);
}

bool Quad::find_hit(const Ray& r, const Interval& ray_t, QuadHit& out) const noexcept {
    const Float denom = dot(normal_, r.direction());
    const Float t = (d_ - dot(normal_, r.origin())) / denom;
    if (!ray_t.surrounds(t)) return false;

    const Point3 intersection = r.at(t);
    const Vec3 planar_hitpt_vector = intersection - q_;

    const Float alpha = dot(w_, cross(planar_hitpt_vector, v_));
    const Float beta = dot(w_, cross(u_, planar_hitpt_vector));
    if (!is_interior(alpha, beta)) return false;

    out = QuadHit{.t = t, .alpha = alpha, .beta = beta};
    return true;
}

void Quad::fill_record(const Ray& r, const QuadHit& quad_hit, HitRecord& rec) const noexcept {
    rec.t = quad_hit.t;
    rec.p = r.at(quad_hit.t);
    rec.spawn_margin = 0.0_f; // Written explicitly: the record is reused across candidates.
    rec.u = quad_hit.alpha;
    rec.v = quad_hit.beta;
    rec.mat = mat_;
    rec.set_face_normal(r, normal_);
}

} // namespace pt
