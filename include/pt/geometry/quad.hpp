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

class quad final : public hittable {
public:
    quad(const point3& Q, const vec3& u, const vec3& v, const material* mat);

    [[nodiscard]] bool hit(const ray& r, const interval& ray_t, hit_record& rec) const override;

    [[nodiscard]] aabb bounding_box() const override;

    [[nodiscard]] Float pdf_value(const point3& origin, const vec3& direction) const override;

    [[nodiscard]] vec3 random(const point3& origin) const override;

private:
    point3 Q;
    vec3 u, v;
    const material* mat = nullptr;
    aabb bbox;
    vec3 normal;
    Float D{};
    vec3 w;
    Float area{};

    void set_bounding_box();

    [[nodiscard]] bool is_interior(Float a, Float b, hit_record& rec) const;
};

} // namespace pt
