#pragma once
#include "pt/math/color.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include "pt/textures/texture.hpp"

namespace pt {

class solid_color final : public texture {
public:
    explicit solid_color(const color& albedo) : albedo(albedo) {}
    solid_color(Float r, Float g, Float b) : solid_color(color(r, g, b)) {}

    [[nodiscard]] color value(Float u, Float v, const point3& p) const override;

private:
    color albedo;
};

} // namespace pt
