#pragma once
#include "pt/core/hit_record.hpp"
#include "pt/materials/material.hpp"
#include "pt/math/color.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/scalar.hpp"
#include <optional>

namespace pt {

class Metal final : public Material {
public:
    explicit Metal(const Color& albedo, Float fuzz);

    [[nodiscard]] std::optional<ScatterRecord> scatter(const Ray& r_in, const HitRecord& rec) const override;

private:
    Color albedo_;
    Float fuzz_{};
};

} // namespace pt
