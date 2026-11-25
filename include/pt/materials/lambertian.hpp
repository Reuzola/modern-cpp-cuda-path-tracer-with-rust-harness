#pragma once
#include "pt/core/hit_record.hpp"
#include "pt/materials/material.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include "pt/textures/texture.hpp"
#include <optional>

namespace pt {

class Sampler;

class Lambertian final : public Material {
public:
    explicit Lambertian(const Texture* tex) : Material(texture_reads_uv(tex)), tex_(tex) {}

    [[nodiscard]] std::optional<ScatterRecord> scatter(const Vec3& incoming, const HitRecord& rec, Sampler& sampler) const override;

    [[nodiscard]] Float scattering_pdf(const Vec3& incoming, const HitRecord& rec, const Vec3& scattered) const override;

private:
    const Texture* tex_;
};

} // namespace pt
