#pragma once
#include "image_loader.hpp"
#include "interval.hpp"
#include "texture.hpp"
#include "vec3.hpp"
#include <string>

class image_texture : public texture {
    public:
        explicit image_texture(const std::string& filename) : image(filename) {}

        [[nodiscard]] color value(double u, double v, const point3&) const override {
            if (image.width() <= 0 || image.height() <= 0) return color(0.0, 1.0, 1.0);

            u = interval(0, 1).clamp(u);
            v = interval(0, 1).clamp(v);
            v = 1.0 - v;

            const int x = static_cast<int>(u * image.width());
            const int y = static_cast<int>(v * image.height());
            return image.pixel_data(x, y);
        }
    private:
        image_loader image;
};