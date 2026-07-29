#include "pt/sampling/mixture_pdf.hpp"
#include "pt/math/random.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"

namespace pt {

Float MixturePdf::value(const Vec3& direction) const {
    return weight * p1.value(direction) + (1.0_f - weight) * p2.value(direction);
}

Vec3 MixturePdf::generate() const {
    return (random_scalar() < weight) ? p1.generate() : p2.generate();
}

} // namespace pt
