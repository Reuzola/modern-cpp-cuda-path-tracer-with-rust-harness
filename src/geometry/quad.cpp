#include "pt/geometry/quad.hpp"
#include "pt/core/hit_record.hpp"
#include "pt/math/aabb.hpp"
#include "pt/math/constants.hpp"
#include "pt/math/interval.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/robust.hpp"
#include "pt/math/sampler.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include <cmath>

namespace pt {

namespace {

// gamma(4) is the smallest order that left no self-hit in randomized tests on large
// tilted quads near the origin; doubled for headroom.
constexpr int plane_rounding_order = 8;

[[nodiscard]] bool is_interior(Float a, Float b) noexcept {
    static constexpr Interval unit_interval{0, 1};
    return unit_interval.contains(a) && unit_interval.contains(b);
}

} // namespace

QuadShape make_quad_shape(const Point3& q, const Vec3& u, const Vec3& v) noexcept {
    const Vec3 n = cross(u, v);
    const Vec3 normal = unit_vector(n);
    const Vec3 w = n / dot(n, n);
    return QuadShape{.q = q, .u = u, .v = v, .normal = normal, .w = w};
}

Aabb quad_bounds(const QuadShape& quad) noexcept {
    const Aabb bbox_diagonal1 = Aabb(quad.q, quad.q + quad.u + quad.v);
    const Aabb bbox_diagonal2 = Aabb(quad.q + quad.u, quad.q + quad.v);
    return Aabb(bbox_diagonal1, bbox_diagonal2);
}

bool intersect_quad(const Ray& r, const Interval& ray_t, const QuadShape& quad, QuadHit& out) noexcept {
    const Vec3 to_corner = quad.q - r.origin();
    const Float plane_distance = dot(quad.normal, to_corner);

    // Taken from the corner, so the rounding grows with how far the origin is from the quad rather
    // than with the quad's coordinates. An origin within that rounding of the plane cannot be told
    // from a point on it - the ray is leaving the surface or grazing it - so there is nothing to find.
    // Tested before the division: both sides of |t| <= t_error share the same |n . d|, and the sign is
    // left to the interval, so a query reaching behind its origin keeps the hits there.
    if (std::fabs(plane_distance) <= gamma(plane_rounding_order) * dot(component_abs(quad.normal), component_abs(to_corner))) return false;

    const Float t = plane_distance / dot(quad.normal, r.direction());
    if (!ray_t.surrounds(t)) return false;

    // The hit relative to the corner, not through r.at(t): no world-sized coordinate enters.
    const Vec3 planar_hitpt_vector = t * r.direction() - to_corner;

    const Float alpha = dot(quad.w, cross(planar_hitpt_vector, quad.v));
    const Float beta = dot(quad.w, cross(quad.u, planar_hitpt_vector));
    if (!is_interior(alpha, beta)) return false;

    out = QuadHit{.t = t, .alpha = alpha, .beta = beta};
    return true;
}

void fill_quad_record(const Ray& r, const QuadShape& quad, const QuadHit& hit, const Material* mat, HitRecord& rec) noexcept {
    rec.t = hit.t;
    rec.p = quad.q + hit.alpha * quad.u + hit.beta * quad.v; // Rebuilt from the quad's own data; r.at(t) would carry the error of a long t.
    rec.spawn_margin = 0.0_f;                                // Covered by the conservative distance test; written because records are reused.
    rec.u = hit.alpha;
    rec.v = hit.beta;
    rec.mat = mat;
    rec.set_face_normal(r, quad.normal);
}

Quad::Quad(const Point3& q, const Vec3& u, const Vec3& v, const Material* mat)
    : shape_(make_quad_shape(q, u, v)), mat_(mat), bbox_(quad_bounds(shape_)), area_(cross(u, v).length()) {}

bool Quad::hit(const Ray& r, const Interval& ray_t, HitRecord& rec) const {
    QuadHit quad_hit;
    if (!intersect_quad(r, ray_t, shape_, quad_hit)) return false;
    fill_quad_record(r, shape_, quad_hit, mat_, rec);
    return true;
}

Aabb Quad::bounding_box() const { return bbox_; }

Float Quad::pdf_direction(const Point3& origin, const Vec3& direction) const {
    QuadHit quad_hit;
    if (!intersect_quad(Ray(origin, direction), Interval(0.0_f, infinity), shape_, quad_hit)) return 0.0_f;

    const Float distance_squared = quad_hit.t * quad_hit.t * direction.length_squared();
    const Float cosine = std::fabs(dot(direction, shape_.normal)) / direction.length();

    return distance_squared / (cosine * area_);
}

Vec3 Quad::sample_direction(const Point3& origin, Sampler& sampler) const {
    const Float a = sampler.next_scalar();
    const Float b = sampler.next_scalar();
    const Point3 point = shape_.q + a * shape_.u + b * shape_.v;
    return point - origin;
}

} // namespace pt
