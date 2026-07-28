#pragma once
#include "pt/math/vec3.hpp"
#include "pt/textures/texture.hpp"

namespace pt {

class solid_color final : public texture {
public:
    explicit solid_color(const color& albedo) : albedo(albedo) {}
    solid_color(double r, double g, double b) : solid_color(color(r, g, b)) {}

    [[nodiscard]] color value(double u, double v, const point3& p) const override;

private:
    color albedo;
};

} // namespace pt
