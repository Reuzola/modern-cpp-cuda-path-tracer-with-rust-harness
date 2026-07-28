#pragma once
#include "pt/core/hit_record.hpp"
#include "pt/core/hittable.hpp"
#include "pt/math/aabb.hpp"
#include "pt/math/interval.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/vec3.hpp"

namespace pt {

class material;

class sphere final : public hittable {
public:
    sphere(const point3& center1, const point3& center2, double radius, const material* mat);

    sphere(const point3& static_center, double radius, const material* mat);

    [[nodiscard]] aabb bounding_box() const override;

    [[nodiscard]] bool hit(const ray& r, const interval& ray_t, hit_record& rec) const override;

    [[nodiscard]] vec3 random(const point3& origin) const override;

    [[nodiscard]] double pdf_value(const point3& origin, const vec3& direction) const override;

private:
    ray center;
    double radius;
    const material* mat = nullptr;
    aabb bbox;

    [[nodiscard]] static uv_coords get_sphere_uv(const point3& p);

    [[nodiscard]] static vec3 random_to_sphere(double radius, double distance_squared);
};

} // namespace pt
