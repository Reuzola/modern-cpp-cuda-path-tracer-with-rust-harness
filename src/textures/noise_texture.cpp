#include "pt/textures/noise_texture.hpp"
#include "pt/math/vec3.hpp"
#include <cmath>

namespace pt {

color noise_texture::value(double, double, const point3& p) const {
    return color(1.0, 1.0, 1.0) * (0.5 * (1.0 + std::sin(scale * p.z() + 10.0 * noise.turb(p, 7))));
}

} // namespace pt
