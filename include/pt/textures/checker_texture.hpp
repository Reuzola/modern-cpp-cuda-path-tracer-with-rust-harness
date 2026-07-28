#pragma once
#include "pt/math/vec3.hpp"
#include "pt/textures/texture.hpp"
#include <memory>

namespace pt {

class checker_texture : public texture {
public:
    checker_texture(double scale, std::unique_ptr<texture> t1, std::unique_ptr<texture> t2)
        : inv_scale(1.0 / scale), even(std::move(t1)), odd(std::move(t2)) {}

    checker_texture(double scale, const color& c1, const color& c2);

    [[nodiscard]] color value(double u, double v, const point3& p) const override;

private:
    double inv_scale{};
    std::unique_ptr<texture> even;
    std::unique_ptr<texture> odd;
};

} // namespace pt
