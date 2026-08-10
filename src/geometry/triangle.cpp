#include "pt/geometry/triangle.hpp"
#include "pt/core/hit_record.hpp"
#include "pt/math/aabb.hpp"
#include "pt/math/interval.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include <cmath>

namespace pt {

// Uses an out-parameter over std::optional to eliminate hot-path allocation/padding overhead.
bool intersect_triangle(const Ray& r, const Interval& ray_t, const Point3& v0,
                        const Point3& v1, const Point3& v2, TriangleHit& out) noexcept {
    // Moller-Trumbore: solves for (t, b1, b2) by Cramer's rule, without ever forming
    // the plane equation. Two-sided - the sign of the determinant is not tested.

    // Rejects both parallel rays and degenerate (zero-area) triangles: the determinant is proportional to the triangle's area.
    constexpr Float parallel_epsilon = 1e-8_f;

    const Vec3 e1 = v1 - v0;
    const Vec3 e2 = v2 - v0;
    const Vec3 pvec = cross(r.direction(), e2);

    const Float det = dot(e1, pvec);
    if (std::fabs(det) < parallel_epsilon) return false;
    const Float inv_det = 1.0_f / det;

    const Vec3 tvec = r.origin() - v0;
    const Float b1 = dot(tvec, pvec) * inv_det;
    if (b1 < 0 || b1 > 1) return false;

    const Vec3 qvec = cross(tvec, e1);
    const Float b2 = dot(r.direction(), qvec) * inv_det;
    if (b2 < 0 || b1 + b2 > 1) return false;

    const Float t = dot(e2, qvec) * inv_det;
    if (!ray_t.contains(t)) return false;

    out.t = t;
    out.b1 = b1;
    out.b2 = b2;
    out.normal = unit_vector(cross(e1, e2));

    return true;
}

Triangle::Triangle(const Point3& v0, const Point3& v1, const Point3& v2, const Material* mat)
    : v0_(v0), v1_(v1), v2_(v2), mat_(mat) {
    bbox_ = Aabb(Aabb(v0_, v1_), Aabb(v1_, v2_));
}

bool Triangle::hit(const Ray& r, const Interval& ray_t, HitRecord& rec, Sampler&) const {
    TriangleHit tri_hit;
    if (!intersect_triangle(r, ray_t, v0_, v1_, v2_, tri_hit)) return false;

    rec.t = tri_hit.t;
    rec.p = r.at(tri_hit.t);
    rec.u = tri_hit.b1;
    rec.v = tri_hit.b2;
    rec.mat = mat_;
    rec.set_face_normal(r, tri_hit.normal);

    return true;
}

Aabb Triangle::bounding_box() const { return bbox_; }

} // namespace pt
