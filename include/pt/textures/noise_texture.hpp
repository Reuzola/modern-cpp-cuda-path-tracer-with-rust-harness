#pragma once
#include "pt/math/vec3.hpp"
#include "pt/textures/perlin.hpp"
#include "pt/textures/texture.hpp"

namespace pt {

class noise_texture final : public texture {
public:
    explicit noise_texture(double scale) : scale(scale) {}

    [[nodiscard]] color value(double u, double v, const point3& p) const override;

private:
    perlin noise;
    double scale{};
};

} // namespace pt
