#include "pt/materials/material.hpp"
#include "pt/core/hit_record.hpp"
#include "pt/math/color.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"

namespace pt {

Material::~Material() = default;

Color Material::emitted(const Vec3&, const HitRecord&) const {
    return Color(0.0_f, 0.0_f, 0.0_f);
}

Float Material::scattering_pdf(const Vec3&, const HitRecord&, const Vec3&) const {
    return 0.0_f;
}

} // namespace pt
