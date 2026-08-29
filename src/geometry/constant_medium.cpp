#include "pt/geometry/constant_medium.hpp"
#include "pt/core/hit_record.hpp"
#include "pt/core/hittable.hpp"
#include "pt/math/constants.hpp"
#include "pt/math/interval.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/sampler.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include <cassert>
#include <cmath>

namespace pt {

ConstantMedium::ConstantMedium(const Hittable* boundary, Float density, const Material* phase_function)
    : boundary_(boundary), neg_inv_density_(-1.0_f / density), phase_function_(phase_function) { assert(boundary != nullptr); }

bool ConstantMedium::sample_interaction(const Ray& r, const Interval& ray_t, Sampler& sampler, HitRecord& rec) const {
    HitRecord rec1;
    HitRecord rec2;

    if (!boundary_->hit(r, Interval::universe, rec1)) return false;
    if (!boundary_->hit(r, Interval(rec1.t + 0.0001_f, infinity), rec2)) return false;

    if (rec1.t < ray_t.min) rec1.t = ray_t.min;
    if (rec2.t > ray_t.max) rec2.t = ray_t.max;
    if (rec1.t >= rec2.t) return false;

    if (rec1.t < 0) rec1.t = 0;

    const Float ray_length = r.direction().length();
    const Float distance_inside_boundary = (rec2.t - rec1.t) * ray_length;
    const Float hit_distance = neg_inv_density_ * std::log(sampler.next_scalar_positive());
    if (hit_distance > distance_inside_boundary) return false;

    rec.t = rec1.t + hit_distance / ray_length;
    rec.p = r.at(rec.t);
    rec.normal = Vec3(1.0_f, 0.0_f, 0.0_f);
    rec.front_face = true;
    rec.mat = phase_function_;
    return true;
}

} // namespace pt
