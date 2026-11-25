#include "support/reference_hittables.hpp"
#include "pt/core/hit_record.hpp"
#include "pt/core/hittable.hpp"
#include "pt/geometry/triangle.hpp"
#include "pt/math/aabb.hpp"
#include "pt/math/interval.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include <cassert>

namespace pt_test {

using pt::operator""_f;

HittableList::HittableList(const pt::Hittable* object) { add(object); }

bool HittableList::hit(const pt::Ray& r, const pt::Interval& ray_t, pt::HitRecord& rec) const {
    pt::Float closest_so_far = ray_t.max;
    pt::HitRecord temp_rec;

    bool is_hit = false;
    for (const pt::Hittable* object : objects_) {
        if (object->hit(r, pt::Interval(ray_t.min, closest_so_far), temp_rec)) {
            closest_so_far = temp_rec.t;
            rec = temp_rec;
            is_hit = true;
        }
    }
    return is_hit;
}

pt::Aabb HittableList::bounding_box() const { return bbox_; }

void HittableList::clear() {
    objects_.clear();
    bbox_ = pt::Aabb();
}

void HittableList::add(const pt::Hittable* object) {
    assert(object != nullptr);

    bbox_ = pt::Aabb(bbox_, object->bounding_box());
    objects_.push_back(object);
}

Triangle::Triangle(const pt::Point3& v0, const pt::Point3& v1, const pt::Point3& v2, const pt::Material* mat)
    : v0_(v0), v1_(v1), v2_(v2), mat_(mat), bbox_(pt::triangle_bounds(v0, v1, v2)) {}

bool Triangle::hit(const pt::Ray& r, const pt::Interval& ray_t, pt::HitRecord& rec) const {
    pt::TriangleHit tri_hit;
    if (!pt::intersect_triangle(r, ray_t, v0_, v1_, v2_, tri_hit)) return false;

    rec.t = tri_hit.t;
    rec.p = pt::barycentric_point(v0_, v1_, v2_, tri_hit.b1, tri_hit.b2);
    rec.spawn_margin = 0.0_f; // Written explicitly: HittableList reuses one record across candidates.
    rec.u = tri_hit.b1;
    rec.v = tri_hit.b2;
    rec.mat = mat_;
    rec.set_face_normal(r, tri_hit.normal);

    return true;
}

pt::Aabb Triangle::bounding_box() const { return bbox_; }

} // namespace pt_test
