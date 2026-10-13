#include "pt/render/path_integrator.hpp"
#include "pt/core/hit_record.hpp"
#include "pt/core/hittable.hpp"
#include "pt/geometry/constant_medium.hpp"
#include "pt/materials/material.hpp"
#include "pt/math/color.hpp"
#include "pt/math/constants.hpp"
#include "pt/math/interval.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include "pt/sampling/importance_targets.hpp"
#include "pt/sampling/mixture_pdf.hpp"
#include "pt/sampling/pdf.hpp"
#include "pt/sampling/pdf_variant.hpp"
#include "pt/sampling/sampleable_pdf.hpp"
#include "pt/util/overloaded.hpp"
#include "pt/util/stats.hpp"
#include <variant>

namespace pt {

Color PathIntegrator::radiance(const Ray& r, Sampler& sampler) const {
    return trace(r, max_depth_, sampler);
}

Color PathIntegrator::trace(const Ray& r, int depth, Sampler& sampler) const {
    if (depth <= 0) return Color(0, 0, 0);

    HitRecord rec;

    count_ray_query();
    const bool hit_surface = world_.hit(r, Interval(0.0_f, infinity), rec);
    Float t_limit = hit_surface ? rec.t : infinity;
    bool hit_medium{false};

    // Free-flight sampling happens here rather than inside Hittable::hit so that
    // the sampler stream advances with the light path, not with traversal order.
    for (const ConstantMedium& medium : media_) {
        HitRecord candidate;
        if (medium.sample_interaction(r, Interval(0.0_f, t_limit), sampler, candidate)) {
            t_limit = candidate.t;
            rec = candidate;
            hit_medium = true;
        }
    }

    if (!hit_surface && !hit_medium) return background_;

    const Color color_from_emission = rec.mat->emitted(r, rec);

    if (const auto sr = rec.mat->scatter(r, rec, sampler)) {
        const auto shade = [&](const Pdf& p, const Point3& origin) -> Color {
            const Vec3 direction = p.generate(sampler);
            const Float pdf_value = p.value(direction);

            // A direction drawn where its own density is zero - rounding at the horizon, a light's
            // edge, the inside of a light - estimates nothing, and dividing by it would leave the
            // pixel NaN for good. This follows the bounce's last draw, so it shortens no other path.
            if (!(pdf_value > 0)) return Color(0, 0, 0);
            const Ray scattered(origin, direction, r.time());

            const Float scattering_pdf = rec.mat->scattering_pdf(r, rec, scattered);
            const Color sample_color = trace(scattered, depth - 1, sampler);

            return (sr->attenuation * scattering_pdf * sample_color) / pdf_value;
        };

        // clang-format off
        return color_from_emission + std::visit(Overloaded{
            [&](const SpecularBounce& sb) -> Color {
                return sr->attenuation * trace(sb.scattered, depth - 1, sampler);
            },
            [&](const DiffuseBounce& db) -> Color {
                const auto& surface_pdf = as_pdf(db.sampling_pdf);

                // Diffuse light leaves on the side the ray arrived from, which is where the geometric
                // normal faces; a volume's zero normal leaves p itself. The bounce and the light pdf
                // share this one point - a light judged from anywhere else misjudges its own cone.
                const Point3 origin = rec.spawn_origin(rec.geometric_normal);

                if (targets_.empty()) return shade(surface_pdf, origin);

                const SampleablePdf target_pdf(targets_, origin);
                const MixturePdf mixed_pdf(surface_pdf, target_pdf);
                return shade(mixed_pdf, origin);
            }
        }, sr->bounce);
        // clang-format on
    }
    return color_from_emission;
}

} // namespace pt
