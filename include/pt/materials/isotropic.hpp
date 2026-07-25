#pragma once
#include "pt/core/hit_record.hpp"
#include "pt/materials/material.hpp"
#include "pt/math/constants.hpp"
#include "pt/math/ray.hpp"
#include "pt/sampling/sphere_pdf.hpp"
#include "pt/textures/texture.hpp"

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
