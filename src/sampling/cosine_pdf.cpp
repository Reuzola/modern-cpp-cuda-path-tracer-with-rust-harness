#include "pt/sampling/cosine_pdf.hpp"
#include "pt/math/constants.hpp"
#include "pt/math/onb.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include <algorithm>

namespace pt {

Float CosinePdf::value(const Vec3& direction) const {
    const Vec3 unit_direction = unit_vector(direction);
    const Float cosine_theta = dot(uvw_.w(), unit_direction);

    return std::max(0.0_f, cosine_theta) / pi;
}

Vec3 CosinePdf::generate() const {
    return uvw_.transform(random_cosine_direction());
}

} // namespace pt
