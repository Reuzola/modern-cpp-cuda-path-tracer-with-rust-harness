#include "pt/sampling/sampleable_pdf.hpp"
#include "pt/core/sampleable.hpp"
#include "pt/math/sampler.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"

namespace pt {

Float SampleablePdf::value(const Vec3& direction) const {
    return target_.pdf_direction(origin_, direction);
}

Vec3 SampleablePdf::generate(Sampler& sampler) const {
    return target_.sample_direction(origin_, sampler);
}

} // namespace pt
