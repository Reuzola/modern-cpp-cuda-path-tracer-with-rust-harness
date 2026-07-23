#pragma once
#include "constants.hpp"
#include "material.hpp"
#include "hit_record.hpp"
#include "sphere_pdf.hpp"
#include "texture.hpp"
#include "ray.hpp"

class isotropic final : public material {
    public:
        explicit isotropic(const texture* tex) : tex(tex) {}

        [[nodiscard]] std::optional<scatter_record> scatter(const ray&, const hit_record& rec) const override {
            return scatter_record{.attenuation = tex->value(rec.u, rec.v, rec.p), .bounce = diffuse_bounce{.sampling_pdf = sphere_pdf()}};
        }

        [[nodiscard]] double scattering_pdf(const ray&, const hit_record&, const ray&) const override {
            return 1.0 / (4.0 * pi);
        }
    private:
        const texture* tex;
};