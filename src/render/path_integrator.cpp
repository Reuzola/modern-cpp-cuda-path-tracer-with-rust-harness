#include "pt/render/path_integrator.hpp"
#include "pt/core/hit_record.hpp"
#include "pt/geometry/constant_medium.hpp"
#include "pt/geometry/top_level_bvh.hpp"
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
#include <optional>
#include <variant>

namespace pt {

namespace {

// Where a path goes after a vertex, and what that bounce multiplies its throughput by. Not a
// Ray: the loop builds its ray in place from these, and a bounce never changes the path's time.
struct Bounce {
    Point3 origin;
    Vec3 direction;
    Color weight;
};

[[nodiscard]] std::optional<Bounce> sample_diffuse(const Pdf& pdf, const Point3& origin, const Vec3& incoming,
                                                   const HitRecord& rec, const Color& attenuation, Sampler& sampler) {
    const Vec3 direction = pdf.generate(sampler);
    const Float pdf_value = pdf.value(direction);

    // A direction drawn where its own density is zero - rounding at the horizon, a light's
    // edge, the inside of a light - estimates nothing, and dividing by it would leave the
    // pixel NaN for good. The path ends here and keeps what it has gathered.
    if (!(pdf_value > 0)) return std::nullopt;

    const Float scattering_pdf = rec.mat->scattering_pdf(incoming, rec, direction);

    // The densities divide as scalars first: a pdf that matches the material gives about one, and the colour is scaled once.
    return Bounce{.origin = origin, .direction = direction, .weight = attenuation * (scattering_pdf / pdf_value)};
}

[[nodiscard]] std::optional<Bounce> sample_bounce(const Vec3& incoming, const HitRecord& rec, const ScatterRecord& sr,
                                                  const ImportanceTargets& targets, Sampler& sampler) {
    // clang-format off
    return std::visit(Overloaded{
        [&](const SpecularBounce& sb) -> std::optional<Bounce> {
            // Which side the bounce leaves from follows the direction: back off a mirror, through a pane of glass.
            return Bounce{.origin = rec.spawn_origin(sb.direction), .direction = sb.direction, .weight = sr.attenuation};
        },
        [&](const DiffuseBounce& db) -> std::optional<Bounce> {
            const Pdf& surface_pdf = as_pdf(db.sampling_pdf);

            // Diffuse light leaves on the side the ray arrived from, which is where the geometric
            // normal faces; a volume's zero normal leaves p itself. The bounce and the light pdf
            // share this one point - a light judged from anywhere else misjudges its own cone.
            const Point3 origin = rec.spawn_origin(rec.geometric_normal);

            if (targets.empty()) return sample_diffuse(surface_pdf, origin, incoming, rec, sr.attenuation, sampler);

            const SampleablePdf target_pdf(targets, origin);
            const MixturePdf mixed_pdf(surface_pdf, target_pdf);
            return sample_diffuse(mixed_pdf, origin, incoming, rec, sr.attenuation, sampler);
        },
    }, sr.bounce);
    // clang-format on
}

} // namespace

Color PathIntegrator::radiance(Ray ray, Sampler& sampler) const {
    // Built front to back: each vertex adds its emission scaled by the product of the bounces
    // before it, so however long the path gets, this is all the state it carries.
    Color throughput(1, 1, 1);
    Color estimate(0, 0, 0);

    for (int depth = 0; depth < max_depth_; ++depth) {
        HitRecord rec;
        if (!next_interaction(ray, sampler, rec)) {
            estimate += throughput * background_;
            break;
        }

        estimate += throughput * rec.mat->emitted(ray.direction(), rec);

        // The budget counts segments and this vertex ends the last one: a bounce sampled here
        // would never be traced, so nothing is drawn for it.
        if (depth + 1 == max_depth_) break;

        const std::optional<ScatterRecord> sr = rec.mat->scatter(ray.direction(), rec, sampler);
        if (!sr) break;

        const std::optional<Bounce> bounce = sample_bounce(ray.direction(), rec, *sr, targets_, sampler);
        if (!bounce) break;

        throughput *= bounce->weight;

        // Nothing past here can reach the camera: a light sample the material does not scatter
        // into, a black albedo, an underflow. Every later term would be exactly zero.
        if (throughput.is_black()) break;
        ray = Ray(bounce->origin, bounce->direction, ray.time());
    }

    return estimate;
}

bool PathIntegrator::next_interaction(const Ray& r, Sampler& sampler, HitRecord& rec) const {
    count_ray_query();
    SurfaceHit surface;
    const bool hit_surface = world_.intersect(r, Interval(0.0_f, infinity), surface);
    Float t_limit = hit_surface ? surface.t() : infinity;

    // Free-flight sampling happens here rather than inside the world query so that
    // the sampler stream advances with the light path, not with traversal order.
    const ConstantMedium* scattering = nullptr;

    // Each medium sees only the segment in front of the nearest event so far, so the last one to report is the nearest.
    for (const ConstantMedium& medium : media_) {
        if (const std::optional<Float> t = medium.sample_distance(r, Interval(0.0_f, t_limit), sampler)) {
            t_limit = *t;
            scattering = &medium;
        }
    }

    if (scattering != nullptr) {
        scattering->fill(r, t_limit, rec);
        return true;
    }

    if (!hit_surface) return false;
    world_.fill(surface, r, rec);
    return true;
}

} // namespace pt
