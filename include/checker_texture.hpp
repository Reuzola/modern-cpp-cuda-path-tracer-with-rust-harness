#pragma once
#include "texture.hpp"
#include "solid_color.hpp"
#include "vec3.hpp"
#include <cmath>
#include <memory>

class checker_texture : public texture {
    public:
        checker_texture(double scale, std::unique_ptr<texture> t1, std::unique_ptr<texture> t2)
        : inv_scale(1.0 / scale), even(std::move(t1)), odd(std::move(t2)) {}
        checker_texture(double scale, const color& c1, const color& c2)
        : checker_texture(scale, std::make_unique<solid_color>(c1), std::make_unique<solid_color>(c2)) {}

        [[nodiscard]] color value(double u, double v, const point3& p) const override {
            const auto x_cell = static_cast<int>(std::floor(inv_scale * p.x()));
            const auto y_cell = static_cast<int>(std::floor(inv_scale * p.y()));
            const auto z_cell = static_cast<int>(std::floor(inv_scale * p.z()));
            const auto sum_cells = x_cell + y_cell + z_cell;

            if (sum_cells % 2 == 0) return even->value(u, v, p);
            return odd->value(u, v, p);
        }
    private:
        double inv_scale{};
        std::unique_ptr<texture> even;
        std::unique_ptr<texture> odd;
};