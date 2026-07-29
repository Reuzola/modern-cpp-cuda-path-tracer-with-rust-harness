#include "pt/geometry/sphere.hpp"
#include "pt/core/hit_record.hpp"
#include "pt/math/aabb.hpp"
#include "pt/math/constants.hpp"
#include "pt/math/interval.hpp"
#include "pt/math/onb.hpp"
#include "pt/math/random.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include <cmath>

namespace pt {

sphere::sphere(const point3& center1, const point3& center2, Float radius, const material* mat) : center(center1, center2 - center1), radius(std::fmax(0.0_f, radius)), mat(mat) {
    const auto rvec = vec3(this->radius, this->radius, this->radius);
    const aabb box1(center.at(0) - rvec, center.at(0) + rvec);
    const aabb box2(center.at(1) - rvec, center.at(1) + rvec);
    bbox = aabb(box1, box2);
}

sphere::sphere(const point3& static_center, Float radius, const material* mat) : sphere(static_center, static_center, radius, mat) {}

aabb sphere::bounding_box() const { return bbox; }

bool sphere::hit(const ray& r, const interval& ray_t, hit_record& rec) const {
    const auto current_center = center.at(r.time());

    const auto oc = current_center - r.origin();
    const Float a = r.direction().length_squared();
    const Float h = dot(oc, r.direction());
    const Float c = oc.length_squared() - (radius * radius);

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
    const vec3 outward_normal = (rec.p - current_center) / radius;
    rec.set_face_normal(r, outward_normal);
    const auto [u, v] = get_sphere_uv(outward_normal);
    rec.u = u;
    rec.v = v;
    rec.mat = mat;

    return true;
}

vec3 sphere::random(const point3& origin) const {
    const vec3 direction = center.at(0) - origin;
    const Float distance_squared = direction.length_squared();

    const onb uvw(direction);
    return uvw.transform(random_to_sphere(radius, distance_squared));
}

Float sphere::pdf_value(const point3& origin, const vec3& direction) const {
    hit_record rec;
    if (!this->hit(ray(origin, direction), interval(0.001_f, infinity), rec)) return 0.0_f;

    const Float distance_squared = (center.at(0) - origin).length_squared();
    if (distance_squared <= radius * radius) return 0.0_f;

    const Float cos_theta_max = std::sqrt(1.0_f - radius * radius / distance_squared);
    const Float solid_angle = 2.0_f * pi * (1.0_f - cos_theta_max);

    return 1.0_f / solid_angle;
}

auto sphere::get_sphere_uv(const point3& p) -> uv_coords {
    const Float theta = std::acos(-p.y());
    const Float phi = std::atan2(-p.z(), p.x()) + pi;

    const Float u = phi / (2 * pi);
    const Float v = theta / pi;
    return {u, v};
}

vec3 sphere::random_to_sphere(Float radius, Float distance_squared) {
    const Float r1 = random_double();
    const Float r2 = random_double();

    const Float z = 1.0_f + r2 * (std::sqrt(1.0_f - radius * radius / distance_squared) - 1.0_f);
    const Float phi = 2.0_f * pi * r1;
    const Float sin_theta = std::sqrt(1.0_f - z * z);

    return vec3(std::cos(phi) * sin_theta, std::sin(phi) * sin_theta, z);
}

} // namespace pt
