#include "pt/core/hittable.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"

namespace pt {

Float hittable::pdf_value(const point3& /*origin*/, const vec3& /*direction*/) const { return 0.0_f; }

vec3 hittable::random(const point3& /*origin*/) const { return vec3(1, 0, 0); }

hittable::~hittable() = default;

} // namespace pt
