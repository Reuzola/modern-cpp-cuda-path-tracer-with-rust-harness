#pragma once
#include "pt/math/color.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include "pt/textures/perlin.hpp"
#include "pt/textures/texture.hpp"

namespace pt {

class noise_texture final : public texture {
public:
    explicit noise_texture(Float scale) : scale(scale) {}

    [[nodiscard]] color value(Float u, Float v, const point3& p) const override;

private:
    perlin noise;
    Float scale{};
};

} // namespace pt
