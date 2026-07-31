#include "pt/geometry/rotate_y.hpp"
#include "pt/core/hit_record.hpp"
#include "pt/core/hittable.hpp"
#include "pt/math/aabb.hpp"
#include "pt/math/constants.hpp"
#include "pt/math/interval.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/sampler.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include <cmath>

namespace pt {

RotateY::RotateY(const Hittable* object, Float angle) : object_(object) {
    const Float radians = degrees_to_radians(angle);
    sin_theta_ = std::sin(radians);
    cos_theta_ = std::cos(radians);

    const Aabb box = object_->bounding_box();
    Point3 min_pt(infinity, infinity, infinity);
    Point3 max_pt(-infinity, -infinity, -infinity);

    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 2; j++) {
            for (int k = 0; k < 2; k++) {
                const Float x = i ? box.x.max : box.x.min;
                const Float y = j ? box.y.max : box.y.min;
                const Float z = k ? box.z.max : box.z.min;

                const Float newx = cos_theta_ * x + sin_theta_ * z;
                const Float newz = -sin_theta_ * x + cos_theta_ * z;

                const Vec3 tester(newx, y, newz);
                for (int c = 0; c < 3; c++) {
                    min_pt[c] = std::fmin(min_pt[c], tester[c]);
                    max_pt[c] = std::fmax(max_pt[c], tester[c]);
                }
            }
        }
    }

    bbox_ = Aabb(min_pt, max_pt);
}

Aabb RotateY::bounding_box() const { return bbox_; }

bool RotateY::hit(const Ray& r, const Interval& ray_t, HitRecord& rec, Sampler& sampler) const {
    Float x_prime = cos_theta_ * r.origin().x() - sin_theta_ * r.origin().z();
    Float z_prime = sin_theta_ * r.origin().x() + cos_theta_ * r.origin().z();
    const Point3 origin(x_prime, r.origin().y(), z_prime);

    x_prime = cos_theta_ * r.direction().x() - sin_theta_ * r.direction().z();
    z_prime = sin_theta_ * r.direction().x() + cos_theta_ * r.direction().z();
    const Vec3 direction(x_prime, r.direction().y(), z_prime);

    const Ray rotated_r(origin, direction, r.time());
    if (!object_->hit(rotated_r, ray_t, rec, sampler)) return false;

    x_prime = cos_theta_ * rec.p.x() + sin_theta_ * rec.p.z();
    z_prime = -sin_theta_ * rec.p.x() + cos_theta_ * rec.p.z();
    const Point3 new_p(x_prime, rec.p.y(), z_prime);
    rec.p = new_p;

    x_prime = cos_theta_ * rec.normal.x() + sin_theta_ * rec.normal.z();
    z_prime = -sin_theta_ * rec.normal.x() + cos_theta_ * rec.normal.z();
    const Vec3 new_n(x_prime, rec.normal.y(), z_prime);
    rec.normal = new_n;

    return true;
}

} // namespace pt
