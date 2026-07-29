#pragma once
#include "pt/math/color.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include "pt/textures/texture.hpp"

namespace pt {

class SolidColor final : public Texture {
public:
    explicit SolidColor(const Color& albedo) : albedo(albedo) {}
    SolidColor(Float r, Float g, Float b) : SolidColor(Color(r, g, b)) {}

    [[nodiscard]] Color value(Float u, Float v, const Point3& p) const override;

private:
    Color albedo;
};

} // namespace pt
