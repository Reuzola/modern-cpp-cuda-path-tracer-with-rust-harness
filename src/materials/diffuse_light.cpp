#include "pt/materials/diffuse_light.hpp"
#include "pt/core/hit_record.hpp"
#include "pt/materials/material.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/vec3.hpp"
#include "pt/textures/texture.hpp"
#include <optional>

namespace pt {

std::optional<scatter_record> diffuse_light::scatter(const ray&, const hit_record&) const {
    return std::nullopt;
}

color diffuse_light::emitted(const ray&, const hit_record& rec) const {
    if (!rec.front_face) return color(0.0, 0.0, 0.0);
    return tex->value(rec.u, rec.v, rec.p);
}

} // namespace pt
