#pragma once
#include "pt/math/color.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include "pt/textures/perlin.hpp"
#include "pt/textures/texture.hpp"

namespace pt {

class NoiseTexture final : public Texture {
public:
    explicit NoiseTexture(Float scale) : scale(scale) {}

    [[nodiscard]] Color value(Float u, Float v, const Point3& p) const override;

private:
    Perlin noise;
    Float scale{};
};

} // namespace pt
