#include "pt/sampling/sphere_pdf.hpp"
#include "pt/math/constants.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"

namespace pt {

Float SpherePdf::value(const Vec3&) const {
    return 1.0_f / (4.0_f * pi);
}

Vec3 SpherePdf::generate() const {
    return random_unit_vector();
}

} // namespace pt
