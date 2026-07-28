#include "pt/textures/solid_color.hpp"
#include "pt/math/vec3.hpp"

namespace pt {

color solid_color::value(double, double, const point3&) const { return albedo; }

} // namespace pt
