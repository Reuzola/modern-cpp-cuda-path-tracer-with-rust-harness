#pragma once
#include "material.hpp"
#include "hit_record.hpp"
#include "vec3.hpp"
#include "ray.hpp"

class lambertian : public material {
    public:
        explicit lambertian(const color& albedo) : albedo(albedo) {}

        [[nodiscard]] std::optional<scatter_record> scatter(const ray& r_in, const hit_record& rec) const override {
            auto scatter_direction = rec.normal + random_unit_vector();
            if (scatter_direction.near_zero()) scatter_direction = rec.normal;

            return scatter_record{ .attenuation = albedo, .scattered = ray(rec.p, scatter_direction, r_in.time()) };
        }
    private:
        color albedo;
};