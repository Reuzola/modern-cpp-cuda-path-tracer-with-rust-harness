#include "pt/sampling/hittable_pdf.hpp"
#include "pt/core/hittable.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"

namespace pt {

Float HittablePdf::value(const Vec3& direction) const {
    return objects.pdf_value(origin, direction);
}

Vec3 HittablePdf::generate() const {
    return objects.random(origin);
}

} // namespace pt
