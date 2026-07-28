#include "pt/materials/isotropic.hpp"
#include "pt/core/hit_record.hpp"
#include "pt/math/constants.hpp"
#include "pt/math/ray.hpp"
#include "pt/sampling/sphere_pdf.hpp"
#include "pt/textures/texture.hpp"
#include <optional>

namespace pt {

std::optional<scatter_record> isotropic::scatter(const ray&, const hit_record& rec) const {
    return scatter_record{.attenuation = tex->value(rec.u, rec.v, rec.p), .bounce = diffuse_bounce{.sampling_pdf = sphere_pdf()}};
}

double isotropic::scattering_pdf(const ray&, const hit_record&, const ray&) const {
    return 1.0 / (4.0 * pi);
}

} // namespace pt
