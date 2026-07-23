#pragma once
#include "material.hpp"
#include "hit_record.hpp"
#include "vec3.hpp"
#include "ray.hpp"
#include <algorithm>

class metal final : public material {
    public:
        explicit metal(const color& albedo, double fuzz) : albedo(albedo), fuzz(std::clamp(fuzz, 0.0, 1.0)) {}

        [[nodiscard]] std::optional<scatter_record> scatter(const ray& r_in, const hit_record& rec) const override {
            auto reflected = reflect(unit_vector(r_in.direction()), rec.normal);
            auto scattered_direction = reflected + fuzz * random_unit_vector();

            if (dot(scattered_direction, rec.normal) > 0) {
                return scatter_record{.attenuation = albedo, .bounce = specular_bounce{.scattered = ray(rec.p, scattered_direction, r_in.time())}};
            }
            return std::nullopt;
        }
    private:
        color albedo;
        double fuzz{};
};