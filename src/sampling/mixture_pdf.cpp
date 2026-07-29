#include "pt/sampling/mixture_pdf.hpp"
#include "pt/math/random.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"

namespace pt {

Float MixturePdf::value(const Vec3& direction) const {
    return weight * p1_.value(direction) + (1.0_f - weight) * p2_.value(direction);
}

Vec3 MixturePdf::generate() const {
    return (random_scalar() < weight) ? p1_.generate() : p2_.generate();
}

} // namespace pt
