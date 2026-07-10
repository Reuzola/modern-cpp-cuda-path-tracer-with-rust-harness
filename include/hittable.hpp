#pragma once
#include "interval.hpp"
#include "ray.hpp"
#include "hit_record.hpp"

class hittable {
    public:
        [[nodiscard]] virtual bool hit(const ray& r, const interval& ray_t, hit_record& rec) const = 0;

        virtual ~hittable() = default;
};