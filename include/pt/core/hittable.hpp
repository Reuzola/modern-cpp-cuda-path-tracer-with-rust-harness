#pragma once
#include "pt/core/hit_record.hpp"
#include "pt/math/aabb.hpp"
#include "pt/math/interval.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"

namespace pt {

class Hittable {
public:
    [[nodiscard]] virtual bool hit(const Ray& r, const Interval& ray_t, HitRecord& rec) const = 0;

    [[nodiscard]] virtual Aabb bounding_box() const = 0;

    [[nodiscard]] virtual Float pdf_value(const Point3& origin, const Vec3& direction) const;

    [[nodiscard]] virtual Vec3 random(const Point3& origin) const;

    virtual ~Hittable();
};

} // namespace pt
