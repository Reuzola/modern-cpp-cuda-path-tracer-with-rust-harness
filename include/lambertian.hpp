#pragma once
#include "constants.hpp"
#include "cosine_pdf.hpp"
#include "material.hpp"
#include "hit_record.hpp"
#include "texture.hpp"
#include "vec3.hpp"
#include "ray.hpp"
#include <algorithm>

class lambertian final : public material {
    public:
        explicit lambertian(const texture* tex) : tex(tex) {}

        [[nodiscard]] std::optional<scatter_record> scatter(const ray&, const hit_record& rec) const override {
            return scatter_record{.attenuation = tex->value(rec.u, rec.v, rec.p), .bounce = diffuse_bounce{.sampling_pdf = cosine_pdf(rec.normal)}};
        }

        [[nodiscard]] double scattering_pdf(const ray&, const hit_record& rec, const ray& scattered) const override {
            const double cos_theta = dot(rec.normal, unit_vector(scattered.direction()));
            return std::max(0.0, cos_theta) / pi;
        }
    private:
        const texture* tex;
};