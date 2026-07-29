#pragma once
#include "pt/core/hit_record.hpp"
#include "pt/materials/material.hpp"
#include "pt/math/color.hpp"
#include "pt/math/ray.hpp"
#include <optional>

namespace pt {

class Texture;

class DiffuseLight final : public Material {
public:
    explicit DiffuseLight(const Texture* tex) : tex_(tex) {}

    [[nodiscard]] std::optional<ScatterRecord> scatter(const Ray& r_in, const HitRecord& rec) const override;

    [[nodiscard]] Color emitted(const Ray& r_in, const HitRecord& rec) const override;

private:
    const Texture* tex_;
};

} // namespace pt
