#pragma once
#include "material.hpp"
#include "hit_record.hpp"
#include "random.hpp"
#include "vec3.hpp"
#include "ray.hpp"
#include <cmath>

class dielectric : public material {
    public:
        explicit dielectric(double refraction_index) : refraction_index(refraction_index) {}

        [[nodiscard]] std::optional<scatter_record> scatter(const ray& r_in, const hit_record& rec) const override {
            const double ri = rec.front_face ? (1.0 / refraction_index) : refraction_index; // ri means refraction index ratio
            const auto unit_direction = unit_vector(r_in.direction());

            const auto cos_theta = std::fmin(dot(-unit_direction, rec.normal), 1.0);
            const auto sin_theta = std::sqrt(1 - cos_theta * cos_theta);

            const bool cannot_refract = ri * sin_theta > 1.0;
            const bool should_reflect = cannot_refract || reflectance(cos_theta, ri) > random_double();

            vec3 direction;
            if (should_reflect) direction = reflect(unit_direction, rec.normal);
            else direction = refract(unit_direction, rec.normal, ri);

            return scatter_record{ .attenuation = color(1.0, 1.0, 1.0), .scattered = ray(rec.p, direction, r_in.time()) };
        }
    private:
        double refraction_index{};

        [[nodiscard]] static double reflectance(double cosine, double refraction_index) {
            const auto r_zero = ((1 - refraction_index) / (1 + refraction_index)) * ((1 - refraction_index) / (1 + refraction_index));
            return r_zero + (1 - r_zero) * ((1-cosine) * (1-cosine) * (1-cosine) * (1-cosine) * (1-cosine)); // (1-cosine)^5
        }
};