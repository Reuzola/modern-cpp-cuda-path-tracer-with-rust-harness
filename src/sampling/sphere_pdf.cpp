#include "pt/sampling/sphere_pdf.hpp"
#include "pt/math/constants.hpp"
#include "pt/math/vec3.hpp"

namespace pt {

double sphere_pdf::value(const vec3&) const {
    return 1.0 / (4.0 * pi);
}

vec3 sphere_pdf::generate() const {
    return random_unit_vector();
}

} // namespace pt
