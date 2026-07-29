#include "pt/textures/solid_color.hpp"
#include "pt/math/color.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"

namespace pt {

Color SolidColor::value(Float, Float, const Point3&) const { return albedo_; }

} // namespace pt
