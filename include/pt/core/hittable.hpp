#pragma once
#include "pt/core/hit_record.hpp"
#include "pt/math/aabb.hpp"
#include "pt/math/interval.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/vec3.hpp"

class hittable {
public:
    [[nodiscard]] virtual bool hit(const ray& r, const interval& ray_t, hit_record& rec) const = 0;

    [[nodiscard]] virtual aabb bounding_box() const = 0;

    [[nodiscard]] virtual double pdf_value(const point3& origin, const vec3& direction) const { return 0.0; }

    [[nodiscard]] virtual vec3 random(const point3& origin) const { return vec3(1, 0, 0); }

    virtual ~hittable() = default;
};
