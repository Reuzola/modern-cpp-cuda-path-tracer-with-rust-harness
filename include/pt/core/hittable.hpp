#pragma once
#include "pt/core/hit_record.hpp"
#include "pt/math/aabb.hpp"
#include "pt/math/interval.hpp"
#include "pt/math/ray.hpp"

namespace pt {

class Hittable {
public:
    [[nodiscard]] virtual bool hit(const Ray& r, const Interval& ray_t, HitRecord& rec) const = 0;

    [[nodiscard]] virtual Aabb bounding_box() const = 0;

    virtual ~Hittable();
};

} // namespace pt
