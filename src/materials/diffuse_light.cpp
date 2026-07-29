#include "pt/materials/diffuse_light.hpp"
#include "pt/core/hit_record.hpp"
#include "pt/materials/material.hpp"
#include "pt/math/color.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/scalar.hpp"
#include "pt/textures/texture.hpp"
#include <optional>

namespace pt {

std::optional<ScatterRecord> DiffuseLight::scatter(const Ray&, const HitRecord&) const {
    return std::nullopt;
}

Color DiffuseLight::emitted(const Ray&, const HitRecord& rec) const {
    if (!rec.front_face) return Color(0.0_f, 0.0_f, 0.0_f);
    return tex_->value(rec.u, rec.v, rec.p);
}

} // namespace pt
