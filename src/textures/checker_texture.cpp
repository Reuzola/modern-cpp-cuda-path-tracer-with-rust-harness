#include "pt/textures/checker_texture.hpp"
#include "pt/math/color.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include "pt/textures/solid_color.hpp"
#include <cmath>
#include <memory>

namespace pt {

CheckerTexture::CheckerTexture(Float scale, const Color& c1, const Color& c2)
    : CheckerTexture(scale, std::make_unique<SolidColor>(c1), std::make_unique<SolidColor>(c2)) {}

Color CheckerTexture::value(Float u, Float v, const Point3& p) const {
    const int x_cell = static_cast<int>(std::floor(inv_scale * p.x()));
    const int y_cell = static_cast<int>(std::floor(inv_scale * p.y()));
    const int z_cell = static_cast<int>(std::floor(inv_scale * p.z()));
    const int sum_cells = x_cell + y_cell + z_cell;

    if (sum_cells % 2 == 0) return even->value(u, v, p);
    return odd->value(u, v, p);
}

} // namespace pt
