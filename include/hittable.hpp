#pragma once
#include "interval.hpp"
#include "ray.hpp"
#include "hit_record.hpp"
#include "aabb.hpp"

class hittable {
    public:
        [[nodiscard]] virtual bool hit(const ray& r, const interval& ray_t, hit_record& rec) const = 0;

        [[nodiscard]] virtual aabb bounding_box() const = 0;

        virtual ~hittable() = default;
};