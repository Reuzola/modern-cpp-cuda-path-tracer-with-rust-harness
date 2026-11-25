#pragma once
#include "pt/math/color.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include "pt/textures/texture.hpp"

namespace pt {

class CheckerTexture final : public Texture {
public:
    // Picks its cell from the point and hands uv to the child it picked, so it reads uv exactly when a child does.
    CheckerTexture(Float scale, const Texture* even, const Texture* odd)
        : Texture(texture_reads_uv(even) || texture_reads_uv(odd)), inv_scale_(1.0_f / scale), even_(even), odd_(odd) {}

    [[nodiscard]] Color value(Float u, Float v, const Point3& p) const override;

private:
    Float inv_scale_{};
    const Texture* even_ = nullptr;
    const Texture* odd_ = nullptr;
};

} // namespace pt
