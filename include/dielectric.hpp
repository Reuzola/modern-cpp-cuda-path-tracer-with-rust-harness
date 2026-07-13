#pragma once
#include "material.hpp"
#include "hit_record.hpp"
#include "vec3.hpp"
#include "ray.hpp"

class dielectric : public material {
    public:
        explicit dielectric(double refraction_index) : refraction_index(refraction_index) {}

        [[nodiscard]] std::optional<scatter_record> scatter(const ray& r_in, const hit_record& rec) const override {
            const double ri = rec.front_face ? (1.0 / refraction_index) : refraction_index; // ri means refraction index ratio

            const auto normalized = unit_vector(r_in.direction());
            const auto refracted_direction = refract(normalized, rec.normal, ri);

            return scatter_record{ .attenuation = color(1.0, 1.0, 1.0), .scattered = ray(rec.p, refracted_direction) };
        }
    private:
        double refraction_index{};
};