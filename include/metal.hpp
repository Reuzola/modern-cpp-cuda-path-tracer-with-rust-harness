#pragma once
#include "material.hpp"
#include "hit_record.hpp"
#include "vec3.hpp"
#include "ray.hpp"

class metal : public material {
    public:
        explicit metal(const color& albedo) : albedo(albedo) {}

        [[nodiscard]] std::optional<scatter_record> scatter(const ray& r_in, const hit_record& rec) const override {
            auto reflected = reflect(unit_vector(r_in.direction()), rec.normal);

            return scatter_record{ .attenuation = albedo, .scattered = ray(rec.p, reflected)};
        }
    private:
        color albedo;
};