#pragma once
#include "material.hpp"
#include "hit_record.hpp"
#include "texture.hpp"
#include "vec3.hpp"
#include "ray.hpp"

class lambertian : public material {
    public:
        explicit lambertian(const texture* tex) : tex(tex) {}

        [[nodiscard]] std::optional<scatter_record> scatter(const ray& r_in, const hit_record& rec) const override {
            auto scatter_direction = rec.normal + random_unit_vector();
            if (scatter_direction.near_zero()) scatter_direction = rec.normal;

            return scatter_record{ .attenuation = tex->value(rec.u, rec.v, rec.p), .scattered = ray(rec.p, scatter_direction, r_in.time()) };
        }
    private:
        const texture* tex;
};