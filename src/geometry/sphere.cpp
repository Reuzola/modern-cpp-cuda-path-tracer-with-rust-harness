#include "pt/geometry/sphere.hpp"
#include "pt/core/hit_record.hpp"
#include "pt/math/aabb.hpp"
#include "pt/math/constants.hpp"
#include "pt/math/interval.hpp"
#include "pt/math/onb.hpp"
#include "pt/math/random.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/sampler.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include <cmath>

namespace pt {

Sphere::Sphere(const Point3& center1, const Point3& center2, Float radius, const Material* mat) : center_(center1, center2 - center1), radius_(std::fmax(0.0_f, radius)), mat_(mat) {
    const auto rvec = Vec3(radius_, radius_, radius_);
    const Aabb box1(center_.at(0) - rvec, center_.at(0) + rvec);
    const Aabb box2(center_.at(1) - rvec, center_.at(1) + rvec);
    bbox_ = Aabb(box1, box2);
}

Sphere::Sphere(const Point3& static_center, Float radius, const Material* mat) : Sphere(static_center, static_center, radius, mat) {}

Aabb Sphere::bounding_box() const { return bbox_; }

bool Sphere::hit(const Ray& r, const Interval& ray_t, HitRecord& rec) const {
    const auto current_center = center_.at(r.time());

    const auto oc = current_center - r.origin();
    const Float a = r.direction().length_squared();
    const Float h = dot(oc, r.direction());
    const Float c = oc.length_squared() - (radius_ * radius_);

    const Float discriminant = h * h - a * c;
    if (discriminant < 0) return false;
    const Float sqrtd = std::sqrt(discriminant);

    Float root = (h - sqrtd) / a;
    if (!ray_t.surrounds(root)) {
        root = (h + sqrtd) / a;
        if (!ray_t.surrounds(root)) {
            return false;
        }
    }

    rec.t = root;
    rec.p = r.at(root);
    const Vec3 outward_normal = (rec.p - current_center) / radius_;
    rec.set_face_normal(r, outward_normal);
    const auto [u, v] = get_sphere_uv(outward_normal);
    rec.u = u;
    rec.v = v;
    rec.mat = mat_;

    return true;
}

Float Sphere::pdf_direction(const Point3& origin, const Vec3& direction) const {
    HitRecord rec;
    if (!this->hit(Ray(origin, direction), Interval(0.001_f, infinity), rec)) return 0.0_f;

    const Float distance_squared = (center_.at(0) - origin).length_squared();
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
    const Float theta = std::acos(-p.y());
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

} // namespace pt
