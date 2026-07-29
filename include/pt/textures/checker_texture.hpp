#pragma once
#include "pt/math/color.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include "pt/textures/texture.hpp"
#include <memory>
#include <utility>

namespace pt {

class CheckerTexture final : public Texture {
public:
    CheckerTexture(Float scale, std::unique_ptr<Texture> t1, std::unique_ptr<Texture> t2)
        : inv_scale_(1.0_f / scale), even_(std::move(t1)), odd_(std::move(t2)) {}

    CheckerTexture(Float scale, const Color& c1, const Color& c2);

    [[nodiscard]] Color value(Float u, Float v, const Point3& p) const override;

private:
    Float inv_scale_{};
    std::unique_ptr<Texture> even_;
    std::unique_ptr<Texture> odd_;
};

} // namespace pt
