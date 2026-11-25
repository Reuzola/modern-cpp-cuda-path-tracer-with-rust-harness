#include "pt/geometry/constant_medium.hpp"
#include "pt/core/hit_record.hpp"
#include "pt/geometry/top_level_bvh.hpp"
#include "pt/math/constants.hpp"
#include "pt/math/interval.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/sampler.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include <cmath>
#include <optional>
#include <utility>

namespace pt {

ConstantMedium::ConstantMedium(TopLevelBvh boundary, Float density, const Material* phase_function)
    : boundary_(std::move(boundary)), neg_inv_density_(-1.0_f / density), phase_function_(phase_function) {}

std::optional<Float> ConstantMedium::sample_distance(const Ray& r, const Interval& ray_t, Sampler& sampler) const {
    // The boundary is asked for distances only: nothing on it is ever shaded, so its records would be filled for nothing.
    SurfaceHit enter_hit;
    if (!boundary_.intersect(r, Interval::universe, enter_hit)) return std::nullopt;

    // Open intervals naturally reject the entry hit at its own distance without nudging. This avoids
    // fixed t-offsets, whose actual world-space distance varies with the ray direction's length.
    SurfaceHit exit_hit;
    if (!boundary_.intersect(r, Interval(enter_hit.t(), infinity), exit_hit)) return std::nullopt;

    Float t_enter = enter_hit.t();
    Float t_exit = exit_hit.t();

    if (t_enter < ray_t.min) t_enter = ray_t.min;
    if (t_exit > ray_t.max) t_exit = ray_t.max;
    if (t_enter >= t_exit) return std::nullopt;

    if (t_enter < 0) t_enter = 0;

    const Float ray_length = r.direction().length();
    const Float distance_inside_boundary = (t_exit - t_enter) * ray_length;
    const Float hit_distance = neg_inv_density_ * std::log(sampler.next_scalar_positive());
    if (hit_distance > distance_inside_boundary) return std::nullopt;

    return t_enter + hit_distance / ray_length;
}

void ConstantMedium::fill(const Ray& r, Float t, HitRecord& rec) const noexcept {
    rec.t = t;
    rec.p = r.at(t);
    rec.normal = Vec3(1, 0, 0);

    // No surface here: a zero normal tells spawn_ray to leave the point alone.
    rec.geometric_normal = Vec3();
    rec.spawn_margin = 0.0_f;
    rec.front_face = true;

    // A phase function may read a texture, and a volume has no surface coordinates to give it.
    rec.u = 0.0_f;
    rec.v = 0.0_f;
    rec.mat = phase_function_;
}

} // namespace pt
