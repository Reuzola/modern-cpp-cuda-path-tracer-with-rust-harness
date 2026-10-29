#include "pt/core/hit_record.hpp"
#include "pt/geometry/constant_medium.hpp"
#include "pt/geometry/quad.hpp"
#include "pt/geometry/sphere.hpp"
#include "pt/geometry/top_level_bvh.hpp"
#include "pt/materials/dielectric.hpp"
#include "pt/materials/diffuse_light.hpp"
#include "pt/materials/isotropic.hpp"
#include "pt/materials/lambertian.hpp"
#include "pt/materials/material.hpp"
#include "pt/materials/metal.hpp"
#include "pt/math/color.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/sampler.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include "pt/render/path_integrator.hpp"
#include "pt/sampling/importance_targets.hpp"
#include "pt/textures/solid_color.hpp"
#include "support/test_support.hpp"
#include "support/top_level_support.hpp"
#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <cmath>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace {

using pt::Color;
using pt::ConstantMedium;
using pt::Dielectric;
using pt::DiffuseLight;
using pt::Float;
using pt::HitRecord;
using pt::ImportanceTargets;
using pt::Isotropic;
using pt::Lambertian;
using pt::Metal;
using pt::PathIntegrator;
using pt::Point3;
using pt::Quad;
using pt::Ray;
using pt::Sampler;
using pt::ScatterRecord;
using pt::SolidColor;
using pt::SpecularBounce;
using pt::Sphere;
using pt::TopLevelBvh;
using pt::Vec3;
using pt::operator""_f;
using pt_test::consumed_draws;
using pt_test::make_sampler;
using pt_test::require_color_near;
using pt_test::require_near;
using pt_test::top_level_of;

// Wound so that its normal is -z: a ray travelling towards +z meets its front.
[[nodiscard]] Quad facing_camera(Float z, const pt::Material* mat) {
    return Quad(Point3(-1, -1, z), Vec3(0, 2, 0), Vec3(2, 0, 0), mat);
}

// Wound the other way, for a surface met by a ray travelling towards -z.
[[nodiscard]] Quad facing_away(Float z, const pt::Material* mat) {
    return Quad(Point3(-1, -1, z), Vec3(2, 0, 0), Vec3(0, 2, 0), mat);
}

// A mirror that also glows. Between two of them a path never ends on its own:
// every vertex both emits and continues, so the result is a sum over every vertex
// the budget allows, each weighted by the attenuation of the bounces before it.
class GlowingMirror final : public pt::Material {
public:
    GlowingMirror(const Color& emission, const Color& albedo) : emission_(emission), albedo_(albedo) {}

    [[nodiscard]] std::optional<ScatterRecord> scatter(const Ray& r_in, const HitRecord& rec, Sampler&) const override {
        const Vec3 reflected = pt::reflect(pt::unit_vector(r_in.direction()), rec.normal);
        return ScatterRecord{
            .attenuation = albedo_,
            .bounce = SpecularBounce{.scattered = rec.spawn_ray(reflected, r_in.time())},
        };
    }

    // Both faces glow: the two mirrors face each other, and the test is about
    // weighting, not about which side of a light emits.
    [[nodiscard]] Color emitted(const Ray&, const HitRecord&) const override { return emission_; }

private:
    Color emission_;
    Color albedo_;
};

const Ray forward{Point3(0, 0, 0), Vec3(0, 0, 1)};

const ImportanceTargets no_targets;
const std::span<const ConstantMedium> no_media;

const Color background{0.2_f, 0.4_f, 0.6_f};

} // namespace

TEST_CASE("a path that hits nothing returns the background", "[render][integrator]") {
    const TopLevelBvh empty_world;
    const PathIntegrator integrator{empty_world, no_media, no_targets, background, 8};

    Sampler sampler = make_sampler(111);
    require_color_near(integrator.radiance(forward, sampler), background);

    // Nothing was drawn. A miss is the most common outcome in an open scene, and
    // the sampler is the one piece of state the path carries - advancing it on a
    // miss would couple every pixel's sequence to how much empty space its rays
    // happened to cross.
    REQUIRE(consumed_draws(sampler, 111, 0));
}

TEST_CASE("an exhausted path is black, not background", "[render][integrator]") {
    const Metal mirror{Color(0.5_f, 0.5_f, 0.5_f), 0.0_f};
    const Quad reflector = facing_camera(2.0_f, &mirror);

    const TopLevelBvh world = top_level_of({&reflector});

    // With two segments the reflection leaves the scene and comes back with the
    // background: the mirror does lead somewhere, so a black answer below can
    // only be the budget's doing.
    const PathIntegrator escaping{world, no_media, no_targets, background, 2};
    Sampler first = make_sampler(112);
    require_color_near(escaping.radiance(forward, first), 0.5_f * background);

    // With one, the budget is spent at the mirror. Black rather than the
    // background: an exhausted path is a path whose remaining contribution is
    // unknown, and guessing the background there would add light that no surface
    // emitted. The bias shows up as glowing edges deep inside a Cornell box.
    const PathIntegrator exhausted{world, no_media, no_targets, background, 1};
    Sampler second = make_sampler(129);
    require_color_near(exhausted.radiance(forward, second), Color(0, 0, 0));
}

TEST_CASE("a light ends the path with its emission", "[render][integrator]") {
    const SolidColor bright{Color(4, 3, 2)};
    const DiffuseLight lamp{&bright};
    const Quad light = facing_camera(2.0_f, &lamp);

    const TopLevelBvh world = top_level_of({&light});

    const PathIntegrator integrator{world, no_media, no_targets, background, 8};

    Sampler sampler = make_sampler(113);
    require_color_near(integrator.radiance(forward, sampler), Color(4, 3, 2));

    // A light absorbs: scatter returns nothing, so the emission is the whole
    // answer and the path stops without a further draw.
    REQUIRE(consumed_draws(sampler, 113, 0));
}

TEST_CASE("the back of a light is not a light", "[render][integrator]") {
    const SolidColor bright{Color(4, 3, 2)};
    const DiffuseLight lamp{&bright};
    const Quad turned_away = facing_away(2.0_f, &lamp);

    const TopLevelBvh world = top_level_of({&turned_away});

    const PathIntegrator integrator{world, no_media, no_targets, Color(0, 0, 0), 8};

    Sampler sampler = make_sampler(114);

    // Emission is one-sided and the surface still blocks: the result is black
    // because the light's back face emits nothing and its geometry hides the
    // background behind it.
    require_color_near(integrator.radiance(forward, sampler), Color(0, 0, 0));
}

TEST_CASE("a specular bounce carries its attenuation forward", "[render][integrator]") {
    const SolidColor white{Color(1, 1, 1)};
    const DiffuseLight lamp{&white};
    const Metal mirror{Color(0.5_f, 0.5_f, 0.5_f), 0.0_f};

    const Quad reflector = facing_camera(2.0_f, &mirror);
    const Quad light = facing_away(-2.0_f, &lamp);

    const TopLevelBvh world = top_level_of({&reflector, &light});

    // Depth two: one bounce off the mirror, then the light.
    const PathIntegrator two_bounces{world, no_media, no_targets, Color(0, 0, 0), 2};
    Sampler sampler = make_sampler(115);

    // The mirror reflects the ray straight back the way it came, into a light of
    // unit radiance, so the answer is exactly the albedo. No PDF appears anywhere
    // in that product: a specular bounce is a single direction with no density to
    // divide by, which is why ScatterRecord distinguishes the two cases at all.
    require_color_near(two_bounces.radiance(forward, sampler), Color(0.5_f, 0.5_f, 0.5_f));
    REQUIRE(consumed_draws(sampler, 115, 0));

    // One less bounce and the light is out of reach: the budget is spent on the
    // mirror itself, so the reflection comes back black rather than unlit-grey.
    const PathIntegrator one_bounce{world, no_media, no_targets, Color(0, 0, 0), 1};
    Sampler shallow = make_sampler(116);
    require_color_near(one_bounce.radiance(forward, shallow), Color(0, 0, 0));
}

TEST_CASE("the depth budget can be changed between passes", "[render][integrator]") {
    const SolidColor white{Color(1, 1, 1)};
    const DiffuseLight lamp{&white};
    const Metal mirror{Color(0.5_f, 0.5_f, 0.5_f), 0.0_f};

    const Quad reflector = facing_camera(2.0_f, &mirror);
    const Quad light = facing_away(-2.0_f, &lamp);

    const TopLevelBvh world = top_level_of({&reflector, &light});

    PathIntegrator integrator{world, no_media, no_targets, Color(0, 0, 0), 1};

    Sampler first = make_sampler(117);
    require_color_near(integrator.radiance(forward, first), Color(0, 0, 0));

    // The viewer raises the depth as the image refines, between passes rather
    // than during one. Nothing is cached from the previous setting - the
    // integrator holds no state beyond its references.
    integrator.set_max_depth(2);

    Sampler second = make_sampler(118);
    require_color_near(integrator.radiance(forward, second), Color(0.5_f, 0.5_f, 0.5_f));
}

TEST_CASE("each vertex adds its emission, weighted by the bounces before it", "[render][integrator]") {
    // Short binary fractions throughout: every product and partial sum below is
    // exact in float, whatever order the integrator adds the terms in.
    const Color glow(1.0_f, 2.0_f, 0.5_f);
    const Color albedo(0.5_f, 0.25_f, 0.75_f);
    const GlowingMirror glowing{glow, albedo};

    // Face to face across the origin: a ray along +z bounces between them on the
    // axis and never leaves, so only the budget can end the path.
    const Quad ahead = facing_camera(2.0_f, &glowing);
    const Quad behind = facing_away(-2.0_f, &glowing);

    const TopLevelBvh world = top_level_of({&ahead, &behind});

    // Four segments, four vertices. The background is not black, so mistaking an
    // exhausted budget for an escape would show in the sum.
    const PathIntegrator integrator{world, no_media, no_targets, background, 4};

    Sampler sampler = make_sampler(128);
    const Color result = integrator.radiance(forward, sampler);

    // glow * (1 + a + a^2 + a^3), channel by channel. A vertex too many reads
    // 1.9375 in red, one too few 1.75, and weighting each vertex's emission by
    // its own attenuation as well as the earlier ones 0.9375.
    const Color expected(1.875_f, 2.65625_f, 1.3671875_f);
    INFO("radiance " << pt_test::hex_color(result) << ", expected " << pt_test::hex_color(expected));
    REQUIRE(pt_test::same_bits(result, expected));

    // A chain of polished reflections decides nothing at random.
    REQUIRE(consumed_draws(sampler, 128, 0));
}

TEST_CASE("a path as long as its budget allows does not grow the call stack", "[render][integrator]") {
    const GlowingMirror glowing{Color(1, 1, 1), Color(1, 1, 1)};
    const Metal plain{Color(1, 1, 1), 0.0_f};

    // The same axis as above, with lossless mirrors: the throughput stays exactly one,
    // so nothing fades, nothing underflows, and every vertex of the budget is visited.
    const Quad ahead = facing_camera(2.0_f, &glowing);
    const Quad behind = facing_away(-2.0_f, &plain);

    const TopLevelBvh world = top_level_of({&ahead, &behind});

    // Far deeper than any scene asks for. A tracer that kept one call frame per vertex
    // would need a frame for each of these, and they do not fit in a thread's stack.
    constexpr int budget = 100'000;
    const PathIntegrator integrator{world, no_media, no_targets, background, budget};

    Sampler sampler = make_sampler(130);
    const Color result = integrator.radiance(forward, sampler);

    // Every second vertex glows: 50000 unit terms, a whole number well inside the range
    // where float counts exactly.
    const Color expected(50'000, 50'000, 50'000);
    INFO("radiance " << pt_test::hex_color(result) << ", expected " << pt_test::hex_color(expected));
    REQUIRE(pt_test::same_bits(result, expected));

    REQUIRE(consumed_draws(sampler, 130, 0));
}

TEST_CASE("a path whose throughput reaches zero stops there", "[render][integrator]") {
    const Metal black_mirror{Color(0, 0, 0), 0.0_f};
    const Dielectric glass{1.5_f};

    // The black mirror sends the ray straight back, onto a pane of glass. Glass draws once
    // at every hit to choose between reflecting and refracting, so whether the path went on
    // past the mirror shows in the sampler.
    const Quad mirror_ahead = facing_camera(2.0_f, &black_mirror);
    const Quad glass_behind = facing_away(-2.0_f, &glass);

    const TopLevelBvh world = top_level_of({&mirror_ahead, &glass_behind});

    const PathIntegrator integrator{world, no_media, no_targets, background, 8};

    Sampler sampler = make_sampler(131);

    // Black either way: past a zero factor nothing can add to the estimate.
    require_color_near(integrator.radiance(forward, sampler), Color(0, 0, 0));

    // But the glass was never reached. A path that kept going would have drawn there.
    REQUIRE(consumed_draws(sampler, 131, 0));
}

TEST_CASE("the diffuse estimator weighs a lambertian bounce by one", "[render][integrator]") {
    const SolidColor albedo{Color(0.6_f, 0.6_f, 0.6_f)};
    const Lambertian matte{&albedo};
    const Quad surface = facing_camera(2.0_f, &matte);

    const TopLevelBvh world = top_level_of({&surface});

    // A uniform white background stands in for a uniformly lit sky: whichever
    // direction the bounce takes, it returns the same radiance.
    const PathIntegrator integrator{world, no_media, no_targets, Color(1, 1, 1), 2};

    Sampler sampler = make_sampler(119);

    // attenuation * scattering_pdf * incoming / pdf_value. For a cosine-sampled
    // lambertian the two densities are the same function evaluated on the same
    // direction, so the ratio is exactly one and a single sample already gives
    // the converged answer: the albedo. This is the payoff of importance
    // sampling matching the BRDF, and it is why a diffuse surface under uniform
    // light is noiseless in this renderer.
    require_color_near(integrator.radiance(forward, sampler), Color(0.6_f, 0.6_f, 0.6_f));
}

TEST_CASE("importance sampling changes the samples, not the answer", "[render][integrator]") {
    const SolidColor albedo{Color(0.6_f, 0.6_f, 0.6_f)};
    const Lambertian matte{&albedo};
    const Quad surface = facing_camera(2.0_f, &matte);

    const TopLevelBvh world = top_level_of({&surface});

    // A target the mixture will aim at. It is deliberately not part of the world:
    // rays sent towards it pass through and reach the same uniform background, so
    // the correct answer is unchanged and only the density used to draw them
    // differs.
    const Quad target = facing_away(-3.0_f, &matte);
    ImportanceTargets targets;
    targets.add(&target);

    const PathIntegrator mixed{world, no_media, targets, Color(1, 1, 1), 2};

    Sampler sampler = make_sampler(120);
    Float total{0};
    constexpr int samples = 8192;

    for (int i = 0; i < samples; ++i) {
        total += mixed.radiance(forward, sampler).r();
    }

    // Half the directions now come from the target's solid angle instead of the
    // cosine lobe, and each carries a different weight - but the estimator stays
    // unbiased, so the mean is still the albedo. A mismatched PDF here is the
    // classic light-sampling bug: it produces a picture that is smooth, plausible
    // and the wrong brightness.
    const Float mean = total / static_cast<Float>(samples);
    require_near(mean, 0.6_f, 0.02);

    // And it really did take the other branch: the mixture draws to choose one.
    Sampler counted = make_sampler(121);
    static_cast<void>(mixed.radiance(forward, counted));
    REQUIRE_FALSE(consumed_draws(counted, 121, 1));
}

TEST_CASE("a medium in front of a surface hides it", "[render][integrator]") {
    const SolidColor white{Color(1, 1, 1)};
    const SolidColor black{Color(0, 0, 0)};
    const DiffuseLight lamp{&white};
    const Isotropic soot{&black};

    const Quad light = facing_camera(5.0_f, &lamp);
    const TopLevelBvh world = top_level_of({&light});

    // Dense enough that a free path of two units is never sampled: the mean free
    // path is a fiftieth of a unit.
    const Sphere boundary{Point3(0, 0, 2), 1.0_f, nullptr};
    std::vector<ConstantMedium> media;
    media.emplace_back(top_level_of({&boundary}), 50.0_f, &soot);

    const PathIntegrator integrator{world, media, no_targets, Color(0, 0, 0), 8};

    Sampler sampler = make_sampler(122);

    // The scattering event replaces the surface hit, and a black phase function
    // absorbs the path there. Free-flight sampling happens in the integrator
    // rather than inside the world query precisely so it can compete with the
    // surface on distance like this.
    require_color_near(integrator.radiance(forward, sampler), Color(0, 0, 0));
}

TEST_CASE("a medium behind a surface is not sampled", "[render][integrator]") {
    const SolidColor white{Color(1, 1, 1)};
    const SolidColor black{Color(0, 0, 0)};
    const DiffuseLight lamp{&white};
    const Isotropic soot{&black};

    const Quad light = facing_camera(5.0_f, &lamp);
    const TopLevelBvh world = top_level_of({&light});

    // The same dense fog, moved behind the light.
    const Sphere boundary{Point3(0, 0, 8), 1.0_f, nullptr};
    std::vector<ConstantMedium> media;
    media.emplace_back(top_level_of({&boundary}), 50.0_f, &soot);

    const PathIntegrator integrator{world, media, no_targets, Color(0, 0, 0), 8};

    Sampler sampler = make_sampler(123);

    // The segment offered to the medium is clipped to the nearest surface, so fog
    // on the far side of a wall costs nothing and changes nothing. Without that
    // clip every medium in the scene would be consulted over an infinite ray and
    // could scatter behind opaque geometry.
    require_color_near(integrator.radiance(forward, sampler), Color(1, 1, 1));
}

TEST_CASE("the same ray and seed give the same radiance", "[render][integrator]") {
    const SolidColor albedo{Color(0.6_f, 0.6_f, 0.6_f)};
    const Lambertian matte{&albedo};
    const Quad surface = facing_camera(2.0_f, &matte);

    const TopLevelBvh world = top_level_of({&surface});

    const PathIntegrator integrator{world, no_media, no_targets, background, 5};

    Sampler first = make_sampler(124);
    Sampler second = make_sampler(124);

    // The integrator holds references and a depth, and nothing else: two calls
    // with equal streams cannot diverge. This is the property that lets the
    // renderer seed per pixel and per pass and get a reproducible image without
    // any coordination between them.
    const Color a = integrator.radiance(forward, first);
    const Color b = integrator.radiance(forward, second);

    REQUIRE(a.r() == b.r());
    REQUIRE(a.g() == b.g());
    REQUIRE(a.b() == b.b());
}

TEST_CASE("a bounce at scene scale does not shadow itself", "[render][integrator]") {
    const SolidColor albedo{Color(0.6_f, 0.6_f, 0.6_f)};
    const Lambertian matte{&albedo};

    // The lambertian estimator above, moved out to where a float coordinate is
    // coarse. The answer is still exactly the albedo: the quad is finite, so a
    // bounce leaving it can only meet the uniform background.
    constexpr Float scale = 10'000.0_f;
    const Quad surface(Point3(scale - 1, scale - 1, scale), Vec3(2, 0.3_f, 0.7_f), Vec3(0.3_f, 2, -0.4_f), &matte);

    const TopLevelBvh world = top_level_of({&surface});

    const PathIntegrator integrator{world, no_media, no_targets, Color(1, 1, 1), 2};

    const Point3 eye(0, 0, 0);
    const Point3 centre(scale + 0.15_f, scale + 0.15_f, scale + 0.15_f);
    const Ray incoming(eye, unit_vector(centre - eye));

    Sampler sampler = make_sampler(125);
    Float total{0};
    constexpr int samples = 256;

    for (int i = 0; i < samples; ++i) {
        total += integrator.radiance(incoming, sampler).r();
    }

    // A self-hit is not a small error: the second bounce lands on the same
    // surface, spends the depth budget and returns black, so the sample is lost
    // outright. Averaged over the hemisphere that shows up as a darkened surface,
    // which is what shadow acne looks like once the pixels are small enough.
    require_near(total / static_cast<Float>(samples), 0.6_f, 0.02);
}

TEST_CASE("a light sample with no density costs its weight, not the pixel", "[render][integrator]") {
    const SolidColor albedo{Color(0.6_f, 0.6_f, 0.6_f)};
    const Lambertian matte{&albedo};
    const Quad surface = facing_camera(2.0_f, &matte);

    const TopLevelBvh world = top_level_of({&surface});

    // A target sphere enclosing the shading point, kept out of the world like the
    // target above. From inside a sphere there is no cone to sample: the light half
    // of the mixture draws a direction that is not a number, to which every density
    // - cosine and light alike - assigns zero. Dividing by that zero once would turn
    // the pixel's running sum into NaN for the rest of the render.
    const Sphere enclosing{Point3(0, 0, 2), 0.5_f, nullptr};
    ImportanceTargets targets;
    targets.add(&enclosing);

    const PathIntegrator integrator{world, no_media, targets, Color(1, 1, 1), 2};

    Sampler sampler = make_sampler(126);
    Float total{0};
    constexpr int samples = 8192;

    for (int i = 0; i < samples; ++i) {
        total += integrator.radiance(forward, sampler).r();
    }
    const Float mean = total / static_cast<Float>(samples);

    // Dropping those samples is still unbiased: the cosine half alone carries the
    // estimate, each of its samples weighted by 1 / 0.5, so the mean stays the
    // albedo. Half the samples are 1.2 and half are 0, which puts one standard
    // deviation at 0.6 / sqrt(8192) = 0.0066; the tolerance is four and a half.
    REQUIRE(std::isfinite(mean));
    require_near(mean, 0.6_f, 0.03);
}

namespace {

// The radiance a camera sees on a large lambertian ground lit by one small sphere
// light, which is both in the world and an importance target. The light stands
// two units from the shading point, sixty degrees off the normal, and subtends a
// cone of half-angle asin(radius / 2).
//
// A sphere's form factor is exact, not a small-light approximation: the ground
// receives E = pi * L * (r / h)^2 * cos(theta) as long as the whole light is above
// its horizon, so the outgoing radiance is albedo * L * (r / h)^2 * cos(theta). The
// light's radiance is set to make that exactly one half at every size.
[[nodiscard]] Float lit_ground_radiance(Float distance_over_radius, std::uint64_t stream, int samples) {
    constexpr Float distance = 2.0_f;
    constexpr Float cos_theta = 0.5_f;
    constexpr Float ground_albedo = 0.5_f;

    const Float radius = distance / distance_over_radius;
    const Float emitted = 0.5_f * distance_over_radius * distance_over_radius / (ground_albedo * cos_theta);

    const SolidColor ground_colour{Color(ground_albedo, ground_albedo, ground_albedo)};
    const SolidColor light_colour{Color(emitted, emitted, emitted)};
    const Lambertian ground_material{&ground_colour};
    const DiffuseLight lamp{&light_colour};

    // The ground sphere of the classic scenes: its top sits at the origin, where the
    // spawn margin is about five ten-thousandths of a unit - a sizeable fraction of
    // the smallest light below.
    const Sphere ground{Point3(0, -1000, 0), 1000.0_f, &ground_material};
    const Sphere light{distance * Point3(0.8660254_f, cos_theta, 0), radius, &lamp};

    const TopLevelBvh world = top_level_of({&ground, &light});

    ImportanceTargets targets;
    targets.add(&light);

    // Depth two: the ground, then the light or the black sky. A path that hits the
    // ground again has shadowed itself and spends its budget there.
    const PathIntegrator integrator{world, no_media, targets, Color(0, 0, 0), 2};
    const Ray down{Point3(0, 3, 0), Vec3(0, -1, 0)};

    Sampler sampler = make_sampler(stream);
    double total = 0.0;
    for (int i = 0; i < samples; ++i) {
        total += pt_test::widen(integrator.radiance(down, sampler).r());
    }
    return pt_test::narrow(total / samples);
}

} // namespace

TEST_CASE("a small tilted light is sampled from the point its rays leave", "[render][integrator]") {
    // Every light-sampled direction aims inside the cone seen from the spawn origin
    // and lands on the light, so half the samples come back at twice the answer and
    // the cosine half almost never finds it: one standard deviation is
    // 0.5 / sqrt(65536) = 0.002, and the tolerance of one percent is two and a half.
    // A pdf evaluated from a different point than the ray left from misjudges the
    // cone's edge, which the tilted light puts across the offset; its error grows
    // as the light shrinks towards the size of the offset itself. At a thousand
    // radii the cone itself has to be computed without cancellation too, or a share
    // of the light samples miss a light the pdf says they aim at.
    const Float distance_over_radius = GENERATE(3.0_f, 10.0_f, 30.0_f, 100.0_f, 300.0_f, 1000.0_f);
    constexpr int samples = 65536;

    const Float mean = lit_ground_radiance(distance_over_radius, 127, samples);

    INFO("d/r " << distance_over_radius << ": mean radiance " << mean);
    REQUIRE(std::isfinite(mean));
    require_near(mean, 0.5_f, 0.005);
}
