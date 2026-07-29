#pragma once
#include "pt/core/hit_record.hpp"
#include "pt/materials/material.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/scalar.hpp"
#include <optional>

namespace pt {

class Texture;

class Isotropic final : public Material {
public:
    explicit Isotropic(const Texture* tex) : tex_(tex) {}

    [[nodiscard]] std::optional<ScatterRecord> scatter(const Ray& r_in, const HitRecord& rec) const override;

    [[nodiscard]] Float scattering_pdf(const Ray& r_in, const HitRecord& rec, const Ray& scattered) const override;

private:
    const Texture* tex_;
};

} // namespace pt
