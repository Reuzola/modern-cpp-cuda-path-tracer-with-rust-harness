#include "pt/sampling/cosine_pdf.hpp"
#include "pt/math/constants.hpp"
#include "pt/math/onb.hpp"
#include "pt/math/vec3.hpp"
#include <algorithm>

namespace pt {

double cosine_pdf::value(const vec3& direction) const {
    const vec3 unit_direction = unit_vector(direction);
    const double cosine_theta = dot(uvw.w(), unit_direction);

    return std::max(0.0, cosine_theta) / pi;
}

vec3 cosine_pdf::generate() const {
    return uvw.transform(random_cosine_direction());
}

} // namespace pt
