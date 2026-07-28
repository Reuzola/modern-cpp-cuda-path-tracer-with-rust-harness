#include "pt/sampling/hittable_pdf.hpp"
#include "pt/core/hittable.hpp"
#include "pt/math/vec3.hpp"

namespace pt {

double hittable_pdf::value(const vec3& direction) const {
    return objects.pdf_value(origin, direction);
}

vec3 hittable_pdf::generate() const {
    return objects.random(origin);
}

} // namespace pt
