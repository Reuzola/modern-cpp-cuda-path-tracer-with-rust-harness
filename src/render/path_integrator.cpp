#include "pt/render/path_integrator.hpp"
#include "pt/core/hit_record.hpp"
#include "pt/core/hittable.hpp"
#include "pt/materials/material.hpp"
#include "pt/math/color.hpp"
#include "pt/math/constants.hpp"
#include "pt/math/interval.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/sampler.hpp"
#include "pt/math/scalar.hpp"
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
    if (!world_.hit(r, Interval(0.001_f, infinity), rec, sampler)) return background_;

    const Color color_from_emission = rec.mat->emitted(r, rec);

    if (const auto sr = rec.mat->scatter(r, rec, sampler)) {
        const auto shade = [&](const Pdf& p) -> Color {
            const Ray scattered(rec.p, p.generate(sampler), r.time());
            const Float pdf_value = p.value(scattered.direction());

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

                if (targets_.empty()) return shade(surface_pdf);

                const SampleablePdf target_pdf(targets_, rec.p);
                const MixturePdf mixed_pdf(surface_pdf, target_pdf);
                return shade(mixed_pdf);
            }
        }, sr->bounce);
        // clang-format on
    }
    return color_from_emission;
}

} // namespace pt
