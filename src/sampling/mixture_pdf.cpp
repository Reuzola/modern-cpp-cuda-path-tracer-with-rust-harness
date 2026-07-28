#include "pt/sampling/mixture_pdf.hpp"
#include "pt/math/random.hpp"
#include "pt/math/vec3.hpp"

namespace pt {

double mixture_pdf::value(const vec3& direction) const {
    return weight * p1.value(direction) + (1.0 - weight) * p2.value(direction);
}

vec3 mixture_pdf::generate() const {
    return (random_double() < weight) ? p1.generate() : p2.generate();
}

} // namespace pt
