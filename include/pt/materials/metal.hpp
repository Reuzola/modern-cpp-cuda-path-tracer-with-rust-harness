#pragma once
#include "pt/core/hit_record.hpp"
#include "pt/materials/material.hpp"
#include "pt/math/color.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include <optional>

namespace pt {

class Sampler;

class Metal final : public Material {
public:
    explicit Metal(const Color& albedo, Float fuzz);

    [[nodiscard]] std::optional<ScatterRecord> scatter(const Vec3& incoming, const HitRecord& rec, Sampler& sampler) const override;

private:
    Color albedo_;
    Float fuzz_{};
};

} // namespace pt
