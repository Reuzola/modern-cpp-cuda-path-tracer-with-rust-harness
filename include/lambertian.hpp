#pragma once
#include "constants.hpp"
#include "material.hpp"
#include "hit_record.hpp"
#include "onb.hpp"
#include "texture.hpp"
#include "vec3.hpp"
#include "ray.hpp"

class lambertian : public material {
    public:
        explicit lambertian(const texture* tex) : tex(tex) {}

        [[nodiscard]] std::optional<scatter_record> scatter(const ray& r_in, const hit_record& rec) const override {
            const onb uvw(rec.normal);
            const vec3 scatter_direction = uvw.transform(random_cosine_direction());

            return scatter_record{ .attenuation = tex->value(rec.u, rec.v, rec.p), .scattered = ray(rec.p, scatter_direction, r_in.time()) };
        }

        [[nodiscard]] double scattering_pdf(const ray&, const hit_record& rec, const ray& scattered) const override {
            const double cos_theta = dot(rec.normal, unit_vector(scattered.direction()));
            return std::max(0.0, cos_theta) / pi;
        }
    private:
        const texture* tex;
};