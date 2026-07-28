#pragma once
#include "pt/core/hit_record.hpp"
#include "pt/materials/material.hpp"
#include "pt/math/ray.hpp"
#include <optional>

namespace pt {

class texture;

class isotropic final : public material {
public:
    explicit isotropic(const texture* tex) : tex(tex) {}

    [[nodiscard]] std::optional<scatter_record> scatter(const ray& r_in, const hit_record& rec) const override;

    [[nodiscard]] double scattering_pdf(const ray& r_in, const hit_record& rec, const ray& scattered) const override;

private:
    const texture* tex;
};

} // namespace pt
