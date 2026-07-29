#include "pt/sampling/sphere_pdf.hpp"
#include "pt/math/constants.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"

namespace pt {

Float sphere_pdf::value(const vec3&) const {
    return 1.0_f / (4.0_f * pi);
}

vec3 sphere_pdf::generate() const {
    return random_unit_vector();
}

} // namespace pt
