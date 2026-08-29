#include "pt/geometry/quad.hpp"
#include "pt/geometry/sphere.hpp"
#include "pt/math/constants.hpp"
#include "pt/math/onb.hpp"
#include "pt/math/sampler.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include "pt/sampling/cosine_pdf.hpp"
#include "pt/sampling/importance_targets.hpp"
#include "pt/sampling/mixture_pdf.hpp"
#include "pt/sampling/pdf.hpp"
#include "pt/sampling/pdf_variant.hpp"
#include "pt/sampling/sampleable_pdf.hpp"
#include "pt/sampling/sphere_pdf.hpp"
#include "support/distribution_test_support.hpp"
#include "support/test_support.hpp"
#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <cmath>

namespace {

using pt::CosinePdf;
using pt::Float;
using pt::ImportanceTargets;
using pt::MixturePdf;
using pt::Onb;
using pt::Pdf;
using pt::Point3;
using pt::Quad;
using pt::SampleablePdf;
using pt::Sampler;
using pt::Sphere;
using pt::SpherePdf;
using pt::Vec3;
using pt::operator""_f;
using pt_test::DistributionCheck;
using pt_test::make_sampler;
using pt_test::require_consistent_distribution;
using pt_test::require_near;
using pt_test::sphere_measure;

// Binds a Pdf's two members into the callables the helpers take.
[[nodiscard]] pt_test::DensityFn density_of(const Pdf& pdf) {
    return [&pdf](const Vec3& direction) { return pdf.value(direction); };
}

[[nodiscard]] pt_test::SampleFn sampler_of(const Pdf& pdf) {
    return [&pdf](Sampler& sampler) { return pdf.generate(sampler); };
}

// Emitters sized to subtend a wide solid angle from the origin. A small light
// would occupy a handful of bins and leave the chi-square without the degrees
// of freedom to say anything: the geometry is chosen for the statistics, and
// the physics it exercises is the same either way.
[[nodiscard]] Quad wide_quad() {
    return Quad(Point3(-2.0_f, 1.0_f, -2.0_f), Vec3(4.0_f, 0.0_f, 0.0_f), Vec3(0.0_f, 0.0_f, 4.0_f), nullptr);
}

[[nodiscard]] Sphere near_sphere() {
    return Sphere(Point3(0.0_f, 2.0_f, 0.0_f), 1.0_f, nullptr);
}

const Point3 shading_point(0.0_f, 0.0_f, 0.0_f);

const Vec3 slanted_normal = unit_vector(Vec3(1.0_f, 2.0_f, -0.5_f));

} // namespace

TEST_CASE("SpherePdf is uniform over the whole sphere", "[sampling][pdf]") {
    const SpherePdf pdf;

    SECTION("the density is the same in every direction") {
        // 1/(4*pi): the only constant that integrates to one over the sphere.
        const Float expected = 1.0_f / (4.0_f * pt::pi);

        require_near(pdf.value(Vec3(1.0_f, 0.0_f, 0.0_f)), expected);
        require_near(pdf.value(Vec3(0.0_f, 0.0_f, -1.0_f)), expected);
        require_near(pdf.value(Vec3(-3.0_f, 7.0_f, 2.0_f)), expected);
    }
    SECTION("value and generate describe the same distribution") {
        require_consistent_distribution(density_of(pdf), sampler_of(pdf),
                                        DistributionCheck{.support_measure = sphere_measure, .stream = 100});
    }
}

TEST_CASE("CosinePdf is cosine-weighted on the hemisphere about its normal", "[sampling][pdf]") {
    const Vec3 normal = GENERATE(Vec3(0.0_f, 0.0_f, 1.0_f), Vec3(0.0_f, -1.0_f, 0.0_f), unit_vector(Vec3(1.0_f, 2.0_f, -0.5_f)));
    const CosinePdf pdf(normal);

    SECTION("the density is cos(theta)/pi against the normal") {
        // Checked at several angles rather than one, so that a density with the
        // right shape but the wrong scale - which the integral test would also
        // catch - is localised to this line.
        require_near(pdf.value(normal), 1.0_f / pt::pi);

        const Vec3 tilted = unit_vector(normal + Vec3(0.4_f, -0.3_f, 0.2_f));
        require_near(pdf.value(tilted), std::max(0.0_f, dot(normal, tilted)) / pt::pi);

        // value() normalises its argument, so the density must not scale with
        // the length of the direction handed to it.
        require_near(pdf.value(9.0_f * tilted), pdf.value(tilted));
    }
    SECTION("the lower hemisphere has zero density") {
        // Clamped at zero rather than left negative: a negative density would
        // come back through the integrator as negative radiance.
        require_near(pdf.value(-normal), 0.0_f);
        require_near(pdf.value(unit_vector(-normal + Vec3(0.1_f, 0.1_f, 0.1_f))), 0.0_f);
    }
    SECTION("a grazing direction sits exactly on the boundary") {
        const Onb basis(normal);
        require_near(pdf.value(basis.u()), 0.0_f);
    }
    SECTION("generate never leaves the upper hemisphere") {
        Sampler sampler = make_sampler(200);
        for (int i = 0; i < 20000; ++i) {
            const Vec3 direction = pdf.generate(sampler);

            require_near(direction.length(), 1.0_f);
            REQUIRE(dot(direction, normal) >= 0.0_f);
        }
    }
    SECTION("value and generate describe the same distribution") {
        // No support measure: 1/p is pi/cos, which grows without bound towards
        // the horizon, so the reciprocal estimator has infinite variance here.
        // The hemispherical support is covered by the case above and by the
        // chi-square, which rejects any sample landing where the density is zero.
        require_consistent_distribution(density_of(pdf), sampler_of(pdf),
                                        DistributionCheck{.stream = 210});
    }
}

TEST_CASE("MixturePdf averages its two components", "[sampling][pdf]") {
    const CosinePdf cosine(slanted_normal);
    const SpherePdf sphere;
    const MixturePdf mixture(cosine, sphere);

    SECTION("the density is the mean of the two") {
        const Vec3 direction = unit_vector(Vec3(0.3_f, 0.6_f, -0.2_f));
        require_near(mixture.value(direction), 0.5_f * (cosine.value(direction) + sphere.value(direction)));
    }
    SECTION("a direction only one component covers still has density") {
        // The point of mixing: the uniform half keeps the density positive
        // where the cosine lobe is blind, which is what stops the integrator
        // dividing by zero on a light sampled below the surface.
        const Vec3 below = -slanted_normal;

        require_near(cosine.value(below), 0.0_f);
        REQUIRE(mixture.value(below) > 0.0_f);
    }
    SECTION("mixing a distribution with itself changes nothing") {
        const MixturePdf doubled(cosine, cosine);
        const Vec3 direction = unit_vector(Vec3(0.3_f, 0.6_f, -0.2_f));

        require_near(doubled.value(direction), cosine.value(direction));
    }
    SECTION("value and generate describe the same distribution") {
        // Support is the whole sphere, and mixing in the uniform half bounds
        // 1/p from above, so the reciprocal estimator is usable here even
        // though it is not for the cosine component on its own.
        require_consistent_distribution(density_of(mixture), sampler_of(mixture),
                                        DistributionCheck{.support_measure = sphere_measure, .stream = 220});
    }
}

TEST_CASE("SampleablePdf turns a quad into a solid-angle density", "[sampling][pdf]") {
    const Quad light = wide_quad();
    const SampleablePdf pdf(light, shading_point);

    SECTION("directions missing the quad have zero density") {
        require_near(pdf.value(Vec3(0.0_f, -1.0_f, 0.0_f)), 0.0_f);
        require_near(pdf.value(Vec3(1.0_f, 0.0_f, 0.0_f)), 0.0_f);
    }
    SECTION("the density matches the solid angle conversion") {
        // Converting an area density to a solid angle one multiplies by the
        // squared distance and divides by the cosine at the light. Straight up
        // the distance is 1, the cosine is 1 and the area is 16.
        require_near(pdf.value(Vec3(0.0_f, 1.0_f, 0.0_f)), 1.0_f / 16.0_f, 1.0e-3);
    }
    SECTION("generate only produces directions the quad covers") {
        Sampler sampler = make_sampler(300);
        for (int i = 0; i < 20000; ++i) {
            REQUIRE(pdf.value(pdf.generate(sampler)) > 0.0_f);
        }
    }
    SECTION("value and generate describe the same distribution") {
        // No support measure: the solid angle a rectangle subtends has a closed
        // form, but restating it here would only test that formula against
        // itself. The integral over the sphere still has to come out at one,
        // and the chi-square checks the shape within the support.
        require_consistent_distribution(density_of(pdf), sampler_of(pdf),
                                        DistributionCheck{.stream = 310});
    }
}

TEST_CASE("SampleablePdf turns a sphere into a solid-angle density", "[sampling][pdf]") {
    const Sphere light = near_sphere();
    const SampleablePdf pdf(light, shading_point);

    // A sphere is sampled uniformly inside the cone it subtends, so the density
    // is constant within that cone: 1/(2*pi*(1 - cos(theta_max))), with
    // sin(theta_max) = radius / distance.
    const Float cos_theta_max = std::sqrt(1.0_f - 1.0_f / 4.0_f);
    const Float density = 1.0_f / (2.0_f * pt::pi * (1.0_f - cos_theta_max));

    SECTION("the density is the reciprocal of the subtended solid angle") {
        require_near(pdf.value(Vec3(0.0_f, 1.0_f, 0.0_f)), density, 1.0e-3);

        // Constant inside the cone, so an off-axis direction that still hits
        // reports the same value rather than a distance-weighted one.
        require_near(pdf.value(unit_vector(Vec3(0.2_f, 1.0_f, 0.1_f))), density, 1.0e-3);
    }
    SECTION("directions outside the cone have zero density") {
        require_near(pdf.value(Vec3(0.0_f, -1.0_f, 0.0_f)), 0.0_f);
        require_near(pdf.value(Vec3(1.0_f, 0.0_f, 0.0_f)), 0.0_f);
    }
    SECTION("value and generate describe the same distribution") {
        // Here the support does have a form simple enough to be an independent
        // check rather than a restatement, so the reciprocal estimator runs.
        const double cone = 2.0 * pt_test::pi_d * (1.0 - std::sqrt(0.75));

        require_consistent_distribution(density_of(pdf), sampler_of(pdf),
                                        DistributionCheck{.support_measure = cone, .stream = 320});
    }
}

TEST_CASE("ImportanceTargets averages the densities of its members", "[sampling][pdf]") {
    const Quad first = wide_quad();
    const Sphere second(Point3(0.0_f, -2.0_f, 0.0_f), 1.0_f, nullptr);

    ImportanceTargets targets;
    targets.add(&first);
    targets.add(&second);

    SECTION("an empty set is inert") {
        // Not an error: most of the scenes have no emitters at all, and the
        // integrator's guard depends on this returning zero rather than
        // dividing by an empty count.
        const ImportanceTargets empty;

        REQUIRE(empty.empty());
        require_near(empty.pdf_direction(shading_point, Vec3(0.0_f, 1.0_f, 0.0_f)), 0.0_f);
    }
    SECTION("the density is the plain average over the targets") {
        // Uniform selection among targets means each contributes 1/n of its own
        // density - including the ones contributing nothing in that direction.
        const Vec3 direction(0.0_f, 1.0_f, 0.0_f);
        const Float expected = 0.5_f * (first.pdf_direction(shading_point, direction) + second.pdf_direction(shading_point, direction));

        require_near(targets.pdf_direction(shading_point, direction), expected);
        REQUIRE(second.pdf_direction(shading_point, direction) == 0.0_f);
    }
    SECTION("value and generate describe the same distribution") {
        // The targets are placed on opposite sides so their supports are
        // disjoint: a direction reaching one of them cannot also reach the
        // other, which makes the halved densities above readable.
        const auto density = [&targets](const Vec3& direction) { return targets.pdf_direction(shading_point, direction); };
        const auto sample = [&targets](Sampler& sampler) { return targets.sample_direction(shading_point, sampler); };

        require_consistent_distribution(density, sample, DistributionCheck{.stream = 400});
    }
}

TEST_CASE("as_pdf dispatches the variant without slicing", "[sampling][pdf]") {
    // The variant lets the integrator hold a pdf by value; as_pdf must hand
    // back a reference into the variant's own storage, not a copy that would
    // have sliced off the derived part.
    const pt::PdfVariant variant{CosinePdf(slanted_normal)};
    const Pdf& pdf = pt::as_pdf(variant);

    require_near(pdf.value(slanted_normal), 1.0_f / pt::pi);
    REQUIRE(&pdf == static_cast<const void*>(&std::get<CosinePdf>(variant)));
}
