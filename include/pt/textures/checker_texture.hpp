#pragma once
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include "pt/textures/texture.hpp"
#include <memory>
#include <utility>

namespace pt {

class checker_texture final : public texture {
public:
    checker_texture(Float scale, std::unique_ptr<texture> t1, std::unique_ptr<texture> t2)
        : inv_scale(1.0_f / scale), even(std::move(t1)), odd(std::move(t2)) {}

    checker_texture(Float scale, const color& c1, const color& c2);

    [[nodiscard]] color value(Float u, Float v, const point3& p) const override;

private:
    Float inv_scale{};
    std::unique_ptr<texture> even;
    std::unique_ptr<texture> odd;
};

} // namespace pt
