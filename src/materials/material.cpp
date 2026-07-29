#include "pt/materials/material.hpp"
#include "pt/core/hit_record.hpp"
#include "pt/math/color.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/scalar.hpp"

namespace pt {

Material::~Material() = default;

Color Material::emitted(const Ray&, const HitRecord&) const {
    return Color(0.0_f, 0.0_f, 0.0_f);
}

Float Material::scattering_pdf(const Ray&, const HitRecord&, const Ray&) const {
    return 0.0_f;
}

} // namespace pt
