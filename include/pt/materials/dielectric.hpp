#pragma once
#include "pt/core/hit_record.hpp"
#include "pt/materials/material.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/scalar.hpp"
#include <optional>

namespace pt {

class dielectric final : public material {
public:
    explicit dielectric(Float refraction_index) : refraction_index(refraction_index) {}

    [[nodiscard]] std::optional<scatter_record> scatter(const ray& r_in, const hit_record& rec) const override;

private:
    Float refraction_index{};

    [[nodiscard]] static Float reflectance(Float cosine, Float refraction_index);
};

} // namespace pt
