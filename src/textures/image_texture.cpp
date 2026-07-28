#include "pt/textures/image_texture.hpp"
#include "pt/io/image_loader.hpp"
#include "pt/math/interval.hpp"
#include "pt/math/vec3.hpp"

namespace pt {

color image_texture::value(double u, double v, const point3&) const {
    if (image.width() <= 0 || image.height() <= 0) return color(0.0, 1.0, 1.0);

    u = interval(0, 1).clamp(u);
    v = interval(0, 1).clamp(v);
    v = 1.0 - v;

    const int x = static_cast<int>(u * image.width());
    const int y = static_cast<int>(v * image.height());
    return image.pixel_data(x, y);
}

} // namespace pt
