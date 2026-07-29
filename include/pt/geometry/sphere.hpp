#pragma once
#include "pt/core/hit_record.hpp"
#include "pt/core/hittable.hpp"
#include "pt/math/aabb.hpp"
#include "pt/math/interval.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"

namespace pt {

class material;

class sphere final : public hittable {
public:
    sphere(const point3& center1, const point3& center2, Float radius, const material* mat);

    sphere(const point3& static_center, Float radius, const material* mat);

    [[nodiscard]] aabb bounding_box() const override;

    [[nodiscard]] bool hit(const ray& r, const interval& ray_t, hit_record& rec) const override;

    [[nodiscard]] vec3 random(const point3& origin) const override;

    [[nodiscard]] Float pdf_value(const point3& origin, const vec3& direction) const override;

private:
    ray center;
    Float radius;
    const material* mat = nullptr;
    aabb bbox;

    struct uv_coords {
        Float u{}, v{};
    };

    [[nodiscard]] static uv_coords get_sphere_uv(const point3& p);

    [[nodiscard]] static vec3 random_to_sphere(Float radius, Float distance_squared);
};

} // namespace pt
