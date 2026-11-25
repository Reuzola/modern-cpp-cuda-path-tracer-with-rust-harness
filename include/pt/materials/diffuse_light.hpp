#pragma once
#include "pt/core/hit_record.hpp"
#include "pt/materials/material.hpp"
#include "pt/math/color.hpp"
#include "pt/math/vec3.hpp"
#include <optional>

namespace pt {

class Texture;
class Sampler;

class DiffuseLight final : public Material {
public:
    explicit DiffuseLight(const Texture* tex) : tex_(tex) {}

    [[nodiscard]] std::optional<ScatterRecord> scatter(const Vec3& incoming, const HitRecord& rec, Sampler& sampler) const override;

    [[nodiscard]] Color emitted(const Vec3& incoming, const HitRecord& rec) const override;

private:
    const Texture* tex_;
};

} // namespace pt
