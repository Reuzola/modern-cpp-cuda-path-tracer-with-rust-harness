#pragma once
#include "pt/math/color.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include "pt/textures/texture.hpp"

namespace pt {

class CheckerTexture final : public Texture {
public:
    CheckerTexture(Float scale, const Texture* even, const Texture* odd)
        : inv_scale_(1.0_f / scale), even_(even), odd_(odd) {}

    [[nodiscard]] Color value(Float u, Float v, const Point3& p) const override;

private:
    Float inv_scale_{};
    const Texture* even_ = nullptr;
    const Texture* odd_ = nullptr;
};

} // namespace pt
