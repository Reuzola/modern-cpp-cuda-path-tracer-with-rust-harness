#include "pt/textures/image_texture.hpp"
#include "pt/io/image_loader.hpp"
#include "pt/math/interval.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"

namespace pt {

color image_texture::value(Float u, Float v, const point3&) const {
    if (image.width() <= 0 || image.height() <= 0) return color(0.0_f, 1.0_f, 1.0_f);

    u = interval(0, 1).clamp(u);
    v = interval(0, 1).clamp(v);
    v = 1.0_f - v;

    const int x = static_cast<int>(u * static_cast<Float>(image.width()));
    const int y = static_cast<int>(v * static_cast<Float>(image.height()));
    return image.pixel_data(x, y);
}

} // namespace pt
