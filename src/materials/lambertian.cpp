#include "pt/materials/lambertian.hpp"
#include "pt/core/hit_record.hpp"
#include "pt/materials/material.hpp"
#include "pt/math/constants.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/vec3.hpp"
#include "pt/sampling/cosine_pdf.hpp"
#include "pt/textures/texture.hpp"
#include <algorithm>
#include <optional>

namespace pt {

std::optional<scatter_record> lambertian::scatter(const ray&, const hit_record& rec) const {
    return scatter_record{.attenuation = tex->value(rec.u, rec.v, rec.p), .bounce = diffuse_bounce{.sampling_pdf = cosine_pdf(rec.normal)}};
}

double lambertian::scattering_pdf(const ray&, const hit_record& rec, const ray& scattered) const {
    const double cos_theta = dot(rec.normal, unit_vector(scattered.direction()));
    return std::max(0.0, cos_theta) / pi;
}

} // namespace pt
