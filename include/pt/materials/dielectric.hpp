#pragma once
#include "pt/core/hit_record.hpp"
#include "pt/materials/material.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/scalar.hpp"
#include <optional>

namespace pt {

class Sampler;

class Dielectric final : public Material {
public:
    explicit Dielectric(Float refraction_index) : refraction_index_(refraction_index) {}

    [[nodiscard]] std::optional<ScatterRecord> scatter(const Ray& r_in, const HitRecord& rec, Sampler& sampler) const override;

private:
    Float refraction_index_{};

    [[nodiscard]] static Float reflectance(Float cosine, Float refraction_index);
};

} // namespace pt
