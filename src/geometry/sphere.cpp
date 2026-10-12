#include "pt/geometry/sphere.hpp"
#include "pt/core/hit_record.hpp"
#include "pt/math/aabb.hpp"
#include "pt/math/constants.hpp"
#include "pt/math/interval.hpp"
#include "pt/math/onb.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/robust.hpp"
#include "pt/math/sampler.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include <algorithm>
#include <cmath>
#include <optional>

namespace pt {

namespace {

// gamma(4) is the smallest order that left no self-hit in randomized tests
// near a large sphere; doubled for headroom.
constexpr int spawn_margin_order = 8;

} // namespace

Sphere::Sphere(const Point3& center1, const Point3& center2, Float radius, const Material* mat)
    : center_(center1, center2 - center1),
      radius_(std::fmax(0.0_f, radius)), mat_(mat),
      spawn_margin_(gamma(spawn_margin_order) * radius_) {
    const auto rvec = Vec3(radius_, radius_, radius_);
    const Aabb box1(center_.at(0) - rvec, center_.at(0) + rvec);
    const Aabb box2(center_.at(1) - rvec, center_.at(1) + rvec);
    bbox_ = Aabb(box1, box2);
}

Sphere::Sphere(const Point3& static_center, Float radius, const Material* mat) : Sphere(static_center, static_center, radius, mat) {}

Aabb Sphere::bounding_box() const { return bbox_; }

bool Sphere::hit(const Ray& r, const Interval& ray_t, HitRecord& rec) const {
    const Point3 center = center_.at(r.time());
    const std::optional<Float> root = find_root(r, center, radius_, ray_t);
    if (!root) return false;
    fill_record(r, center, *root, rec);
    return true;
}

Float Sphere::pdf_direction(const Point3& origin, const Vec3& direction) const {
    const Point3 center = center_.at(0);
    if (!find_root(Ray(origin, direction), center, radius_, Interval(0.0_f, infinity))) return 0.0_f;

    const Float distance_squared = (center - origin).length_squared();
    if (distance_squared <= radius_ * radius_) return 0.0_f;

    const Float cos_theta_max = std::sqrt(1.0_f - radius_ * radius_ / distance_squared);
    const Float solid_angle = 2.0_f * pi * (1.0_f - cos_theta_max);

    return 1.0_f / solid_angle;
}

Vec3 Sphere::sample_direction(const Point3& origin, Sampler& sampler) const {
    const Vec3 direction = center_.at(0) - origin;
    const Float distance_squared = direction.length_squared();

    const Onb uvw(direction);
    return uvw.transform(random_to_sphere(radius_, distance_squared, sampler));
}

auto Sphere::get_sphere_uv(const Point3& p) -> UvCoords {
    // The normal is renormalised, but rounding can still leave a component a few ulps
    // past +/-1 - enough for acos to return NaN at the poles. Clamping is a no-op elsewhere.
    const Float cos_theta = -p.y();
    const Float theta = std::acos(std::clamp(cos_theta, -1.0_f, 1.0_f));
    const Float phi = std::atan2(-p.z(), p.x()) + pi;

    const Float u = phi / (2 * pi);
    const Float v = theta / pi;
    return {u, v};
}

Vec3 Sphere::random_to_sphere(Float radius, Float distance_squared, Sampler& sampler) {
    const Float r1 = sampler.next_scalar();
    const Float r2 = sampler.next_scalar();

    const Float z = 1.0_f + r2 * (std::sqrt(1.0_f - radius * radius / distance_squared) - 1.0_f);
    const Float phi = 2.0_f * pi * r1;
    const Float sin_theta = std::sqrt(1.0_f - z * z);

    return Vec3(std::cos(phi) * sin_theta, std::sin(phi) * sin_theta, z);
}

std::optional<Float> Sphere::find_root(const Ray& r, const Point3& center, Float radius, const Interval& ray_t) noexcept {
    const Vec3 oc = center - r.origin();
    const Float a = r.direction().length_squared();
    const Float h = dot(oc, r.direction());
    const Vec3 offset = oc - (h / a) * r.direction();

    const Float discriminant = a * (radius * radius - offset.length_squared());
    if (discriminant < 0) return std::nullopt;
    const Float sqrtd = std::sqrt(discriminant);

    const Float c = oc.length_squared() - radius * radius;
    const Float q = h < 0 ? h - sqrtd : h + sqrtd;

    const Float near_root = std::fmin(q / a, c / q);
    const Float far_root = std::fmax(q / a, c / q);

    Float root = near_root;
    if (!ray_t.surrounds(root)) {
        root = far_root;
        if (!ray_t.surrounds(root)) {
            return std::nullopt;
        }
    }

    return root;
}

void Sphere::fill_record(const Ray& r, const Point3& center, Float t, HitRecord& rec) const noexcept {
    rec.t = t;

    // Reprojected onto the surface: r.at(t) carries t's error, which grows with the radius (PBRT 6.8.5).
    const Vec3 radial = r.at(t) - center;

    // Divided by the true length, not radius_, so the normal is unit.
    const Vec3 outward_normal = radial / radial.length();

    rec.p = center + radius_ * outward_normal;
    rec.spawn_margin = spawn_margin_;
    rec.set_face_normal(r, outward_normal);
    const auto [u, v] = get_sphere_uv(outward_normal);
    rec.u = u;
    rec.v = v;
    rec.mat = mat_;
}

} // namespace pt
