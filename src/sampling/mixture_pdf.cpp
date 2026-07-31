#include "pt/sampling/mixture_pdf.hpp"
#include "pt/math/sampler.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"

namespace pt {

Float MixturePdf::value(const Vec3& direction) const {
    return weight * p1_.value(direction) + (1.0_f - weight) * p2_.value(direction);
}

Vec3 MixturePdf::generate(Sampler& sampler) const {
    return (sampler.next_scalar() < weight) ? p1_.generate(sampler) : p2_.generate(sampler);
}

} // namespace pt
