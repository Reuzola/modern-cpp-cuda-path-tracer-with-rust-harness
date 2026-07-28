#pragma once
#include "pt/io/image_loader.hpp"
#include "pt/math/vec3.hpp"
#include "pt/textures/texture.hpp"
#include <string>

namespace pt {

class image_texture : public texture {
public:
    explicit image_texture(const std::string& filename) : image(filename) {}

    [[nodiscard]] color value(double u, double v, const point3& p) const override;

private:
    image_loader image;
};

} // namespace pt
