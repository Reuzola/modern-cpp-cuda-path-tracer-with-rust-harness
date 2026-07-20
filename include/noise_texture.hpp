#pragma once
#include "perlin.hpp"
#include "texture.hpp"
#include "vec3.hpp"
#include <cmath>

class noise_texture : public texture {
    public:
        explicit noise_texture(double scale) : scale(scale) {}

        [[nodiscard]] color value(double, double, const point3& p) const override {
            return color(1.0, 1.0, 1.0) * (0.5 * (1.0 + std::sin(scale * p.z() + 10.0 * noise.turb(p, 7))));
        }
    private:
        perlin noise;
        double scale{};
};