#include "pt/textures/noise_texture.hpp"
#include "pt/math/color.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include <cmath>

namespace pt {

color noise_texture::value(Float, Float, const point3& p) const {
    return color(1.0_f, 1.0_f, 1.0_f) * (0.5_f * (1.0_f + std::sin(scale * p.z() + 10.0_f * noise.turb(p, 7))));
}

} // namespace pt
