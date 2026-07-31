#include "pt/render/path_integrator.hpp"
#include "pt/core/hit_record.hpp"
#include "pt/core/hittable.hpp"
#include "pt/materials/material.hpp"
#include "pt/math/color.hpp"
#include "pt/math/constants.hpp"
#include "pt/math/interval.hpp"
#include "pt/math/random.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/scalar.hpp"
#include "pt/sampling/importance_targets.hpp"
#include "pt/sampling/mixture_pdf.hpp"
#include "pt/sampling/pdf.hpp"
#include "pt/sampling/pdf_variant.hpp"
#include "pt/sampling/sampleable_pdf.hpp"
#include "pt/util/overloaded.hpp"
#include <variant>

namespace pt {

Color PathIntegrator::radiance(const Ray& r) const {
    return trace(r, max_depth_);
}

Color PathIntegrator::trace(const Ray& r, int depth) const {
    if (depth <= 0) return Color(0, 0, 0);

    HitRecord rec;

    if (!world_.hit(r, Interval(0.001_f, infinity), rec)) return background_;

    const Color color_from_emission = rec.mat->emitted(r, rec);

    if (const auto sr = rec.mat->scatter(r, rec)) {
        const auto shade = [&](const Pdf& p) -> Color {
            const Ray scattered(rec.p, p.generate(legacy_sampler()), r.time());
            const Float pdf_value = p.value(scattered.direction());

            const Float scattering_pdf = rec.mat->scattering_pdf(r, rec, scattered);
            const Color sample_color = trace(scattered, depth - 1);

            return (sr->attenuation * scattering_pdf * sample_color) / pdf_value;
        };

        // clang-format off
        return color_from_emission + std::visit(Overloaded{
            [&](const SpecularBounce& sb) -> Color {
                return sr->attenuation * trace(sb.scattered, depth - 1);
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
