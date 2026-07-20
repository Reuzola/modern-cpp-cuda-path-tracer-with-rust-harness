#pragma once
#include "perlin.hpp"
#include "texture.hpp"
#include "vec3.hpp"

class noise_texture : public texture {
    public:
        explicit noise_texture(double scale) : scale(scale) {}

        [[nodiscard]] color value(double, double, const point3& p) const override {
            return color(1.0, 1.0, 1.0) * 0.5 * (1.0 + noise.noise(scale * p));
        }
    private:
        perlin noise;
        double scale{};
};