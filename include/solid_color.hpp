#pragma once
#include "vec3.hpp"
#include "texture.hpp"

class solid_color : public texture {
    public:
        explicit solid_color(const color& albedo) : albedo(albedo) {}
        solid_color(double r, double g, double b) : solid_color(color(r, g, b)) {}

        [[nodiscard]] color value(double, double, const point3&) const override { return albedo; }
    private:
        color albedo;
};