#pragma once
#include "ray.hpp"
#include "hit_record.hpp"

class hittable {
    public:
        [[nodiscard]] virtual bool hit(const ray& r, double ray_tmin, double ray_tmax, hit_record& rec) const = 0;

        virtual ~hittable() = default;
};