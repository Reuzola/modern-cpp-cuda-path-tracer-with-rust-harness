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

struct SphereUv {
    Float u{};
    Float v{};
};

[[nodiscard]] SphereUv sphere_uv(const Point3& p) noexcept {
    // The normal is renormalised, but rounding can still leave a component a few ulps
    // past +/-1 - enough for acos to return NaN at the poles. Clamping is a no-op elsewhere.
    const Float cos_theta = -p.y();
    const Float theta = std::acos(std::clamp(cos_theta, -1.0_f, 1.0_f));
    const Float phi = std::atan2(-p.z(), p.x()) + pi;

    const Float u = phi / (2 * pi);
    const Float v = theta / pi;
    return {u, v};
}

// 1 - cos(theta_max) of the cone a sphere subtends, shared by the density and the sampler so the two describe one cone.
[[nodiscard]] Float cone_one_minus_cos(Float radius, Float distance_squared) noexcept {
    const Float x = radius * radius / distance_squared;

    // The same value as 1 - sqrt(1 - x) with nothing subtracted: written that
    // way, a light a thousand radii away is all rounding.
    return x / (1.0_f + std::sqrt(1.0_f - x));
}

[[nodiscard]] Vec3 random_to_sphere(Float radius, Float distance_squared, Sampler& sampler) {
    const Float r1 = sampler.next_scalar();
    const Float r2 = sampler.next_scalar();

    // Drawn as 1 - z: for a distant light z sits within an ulp of one, and
    // anything built from z itself would cancel against it.
    const Float one_minus_z = r2 * cone_one_minus_cos(radius, distance_squared);

    // 1 - z^2 = (1 - z)(1 + z), formed from 1 - z.
    const Float z = 1.0_f - one_minus_z;
    const Float phi = 2.0_f * pi * r1;
    const Float sin_theta = std::sqrt(one_minus_z * (2.0_f - one_minus_z));

    return Vec3(std::cos(phi) * sin_theta, std::sin(phi) * sin_theta, z);
}

} // namespace

Aabb sphere_bounds(const SphereShape& sphere) noexcept {
    const Vec3 rvec(sphere.radius, sphere.radius, sphere.radius);
    const Aabb box1(sphere.center_at(0.0_f) - rvec, sphere.center_at(0.0_f) + rvec);
    const Aabb box2(sphere.center_at(1.0_f) - rvec, sphere.center_at(1.0_f) + rvec);
    return Aabb(box1, box2);
}

std::optional<Float> intersect_sphere(const Ray& r, const Interval& ray_t, const SphereShape& sphere) noexcept {
    const Point3 center = sphere.center_at(r.time());
    const Vec3 oc = center - r.origin();
    const Float a = r.direction().length_squared();
    const Float h = dot(oc, r.direction());
    const Vec3 offset = oc - (h / a) * r.direction();

    const Float discriminant = a * (sphere.radius * sphere.radius - offset.length_squared());
    if (discriminant < 0) return std::nullopt;
    const Float sqrtd = std::sqrt(discriminant);

    const Float c = oc.length_squared() - sphere.radius * sphere.radius;
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

void fill_sphere_record(const Ray& r, const SphereShape& sphere, Float t, const Material* mat, HitRecord& rec) noexcept {
    // Recomputed, not passed in: the expression the test used, so the same centre.
    const Point3 center = sphere.center_at(r.time());
    rec.t = t;

    // Reprojected onto the surface: r.at(t) carries t's error, which grows with the radius (PBRT 6.8.5).
    const Vec3 radial = r.at(t) - center;

    // Divided by the true length, not by the radius, so the normal is unit.
    const Vec3 outward_normal = radial / radial.length();

    rec.p = center + sphere.radius * outward_normal;
    rec.spawn_margin = gamma(spawn_margin_order) * sphere.radius;
    rec.set_face_normal(r, outward_normal);
    const auto [u, v] = sphere_uv(outward_normal);
    rec.u = u;
    rec.v = v;
    rec.mat = mat;
}

Sphere::Sphere(const Point3& center1, const Point3& center2, Float radius, const Material* mat)
    : shape_{.center = center1, .motion = center2 - center1, .radius = std::fmax(0.0_f, radius)},
      mat_(mat), bbox_(sphere_bounds(shape_)) {}

Sphere::Sphere(const Point3& static_center, Float radius, const Material* mat) : Sphere(static_center, static_center, radius, mat) {}

Aabb Sphere::bounding_box() const { return bbox_; }

bool Sphere::hit(const Ray& r, const Interval& ray_t, HitRecord& rec) const {
    const std::optional<Float> root = intersect_sphere(r, ray_t, shape_);
    if (!root) return false;
    fill_sphere_record(r, shape_, *root, mat_, rec);
    return true;
}

Float Sphere::pdf_direction(const Point3& origin, const Vec3& direction) const {
    const Point3 center = shape_.center_at(0.0_f);
    if (!intersect_sphere(Ray(origin, direction), Interval(0.0_f, infinity), shape_)) return 0.0_f;

    const Float distance_squared = (center - origin).length_squared();
    if (distance_squared <= shape_.radius * shape_.radius) return 0.0_f;

    const Float solid_angle = 2.0_f * pi * cone_one_minus_cos(shape_.radius, distance_squared);
    return 1.0_f / solid_angle;
}

Vec3 Sphere::sample_direction(const Point3& origin, Sampler& sampler) const {
    const Vec3 direction = shape_.center_at(0.0_f) - origin;
    const Float distance_squared = direction.length_squared();

    const Onb uvw(direction);
    return uvw.transform(random_to_sphere(shape_.radius, distance_squared, sampler));
}

} // namespace pt
