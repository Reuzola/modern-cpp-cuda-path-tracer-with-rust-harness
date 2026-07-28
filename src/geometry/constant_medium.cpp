#include "pt/geometry/constant_medium.hpp"
#include "pt/core/hit_record.hpp"
#include "pt/core/hittable.hpp"
#include "pt/materials/material.hpp"
#include "pt/math/aabb.hpp"
#include "pt/math/constants.hpp"
#include "pt/math/interval.hpp"
#include "pt/math/random.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/vec3.hpp"
#include <cmath>
#include <memory>

namespace pt {

constant_medium::constant_medium(std::shared_ptr<hittable> boundary, double density, const material* phase_function)
    : boundary(std::move(boundary)), neg_inv_density(-1.0 / density), phase_function(phase_function) {}

aabb constant_medium::bounding_box() const { return boundary->bounding_box(); }

bool constant_medium::hit(const ray& r, const interval& ray_t, hit_record& rec) const {
    hit_record rec1, rec2;

    if (!boundary->hit(r, interval::universe, rec1)) return false;
    if (!boundary->hit(r, interval(rec1.t + 0.0001, infinity), rec2)) return false;

    if (rec1.t < ray_t.min) rec1.t = ray_t.min;
    if (rec2.t > ray_t.max) rec2.t = ray_t.max;
    if (rec1.t >= rec2.t) return false;

    if (rec1.t < 0) rec1.t = 0;

    const double ray_length = r.direction().length();
    const double distance_inside_boundary = (rec2.t - rec1.t) * ray_length;
    const double hit_distance = neg_inv_density * std::log(random_double());
    if (hit_distance > distance_inside_boundary) return false;

    rec.t = rec1.t + hit_distance / ray_length;
    rec.p = r.at(rec.t);
    rec.normal = vec3(1.0, 0.0, 0.0);
    rec.front_face = true;
    rec.mat = phase_function;
    return true;
}

} // namespace pt
