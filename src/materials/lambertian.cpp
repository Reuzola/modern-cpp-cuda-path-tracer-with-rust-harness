#include "pt/materials/lambertian.hpp"
#include "pt/core/hit_record.hpp"
#include "pt/materials/material.hpp"
#include "pt/math/constants.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include "pt/sampling/cosine_pdf.hpp"
#include "pt/textures/texture.hpp"
#include <algorithm>
#include <optional>

namespace pt {

std::optional<ScatterRecord> Lambertian::scatter(const Ray&, const HitRecord& rec) const {
    return ScatterRecord{.attenuation = tex->value(rec.u, rec.v, rec.p), .bounce = DiffuseBounce{.sampling_pdf = CosinePdf(rec.normal)}};
}

Float Lambertian::scattering_pdf(const Ray&, const HitRecord& rec, const Ray& scattered) const {
    const Float cos_theta = dot(rec.normal, unit_vector(scattered.direction()));
    return std::max(0.0_f, cos_theta) / pi;
}

} // namespace pt
