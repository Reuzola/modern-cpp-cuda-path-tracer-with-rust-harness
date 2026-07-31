#pragma once
#include "pt/math/color.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include "pt/textures/perlin.hpp"
#include "pt/textures/texture.hpp"

namespace pt {

class Sampler;

class NoiseTexture final : public Texture {
public:
    NoiseTexture(Float scale, Sampler& sampler) : noise_(sampler), scale_(scale) {}

    [[nodiscard]] Color value(Float u, Float v, const Point3& p) const override;

private:
    Perlin noise_;
    Float scale_{};
};

} // namespace pt
