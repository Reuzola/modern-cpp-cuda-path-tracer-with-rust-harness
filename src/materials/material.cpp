#include "pt/materials/material.hpp"
#include "pt/core/hit_record.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/vec3.hpp"

namespace pt {

material::~material() = default;

color material::emitted(const ray&, const hit_record&) const {
    return color(0.0, 0.0, 0.0);
}

double material::scattering_pdf(const ray&, const hit_record&, const ray&) const {
    return 0.0;
}

} // namespace pt
