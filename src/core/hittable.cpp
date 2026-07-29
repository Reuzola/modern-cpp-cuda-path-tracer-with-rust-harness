#include "pt/core/hittable.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"

namespace pt {

Float Hittable::pdf_value(const Point3& /*origin*/, const Vec3& /*direction*/) const { return 0.0_f; }

Vec3 Hittable::random(const Point3& /*origin*/) const { return Vec3(1, 0, 0); }

Hittable::~Hittable() = default;

} // namespace pt
