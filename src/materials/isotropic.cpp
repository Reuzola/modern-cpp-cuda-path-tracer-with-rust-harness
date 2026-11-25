#include "pt/materials/isotropic.hpp"
#include "pt/core/hit_record.hpp"
#include "pt/materials/material.hpp"
#include "pt/math/constants.hpp"
#include "pt/math/sampler.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include "pt/sampling/sphere_pdf.hpp"
#include "pt/textures/texture.hpp"
#include <optional>

namespace pt {

std::optional<ScatterRecord> Isotropic::scatter(const Vec3&, const HitRecord& rec, Sampler&) const {
    return ScatterRecord{.attenuation = tex_->value(rec.u, rec.v, rec.p), .bounce = DiffuseBounce{.sampling_pdf = SpherePdf()}};
}

Float Isotropic::scattering_pdf(const Vec3&, const HitRecord&, const Vec3&) const {
    return 1.0_f / (4.0_f * pi);
}

} // namespace pt
