#include "pt/textures/image_texture.hpp"
#include "pt/io/image_loader.hpp"
#include "pt/math/color.hpp"
#include "pt/math/interval.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"

namespace pt {

Color ImageTexture::value(Float u, Float v, const Point3&) const {
    if (image_.width() <= 0 || image_.height() <= 0) return Color(0.0_f, 1.0_f, 1.0_f);

    u = Interval(0, 1).clamp(u);
    v = Interval(0, 1).clamp(v);
    v = 1.0_f - v;

    const int x = static_cast<int>(u * static_cast<Float>(image_.width()));
    const int y = static_cast<int>(v * static_cast<Float>(image_.height()));
    return image_.pixel_data(x, y);
}

} // namespace pt
