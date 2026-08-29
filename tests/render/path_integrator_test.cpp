#include "pt/core/hittable_list.hpp"
#include "pt/geometry/constant_medium.hpp"
#include "pt/geometry/quad.hpp"
#include "pt/geometry/sphere.hpp"
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
#include <catch2/catch_test_macros.hpp>
#include <span>
#include <vector>

namespace {

using pt::Color;
using pt::ConstantMedium;
using pt::DiffuseLight;
using pt::Float;
using pt::HittableList;
using pt::ImportanceTargets;
using pt::Isotropic;
using pt::Lambertian;
using pt::Metal;
using pt::PathIntegrator;
using pt::Point3;
using pt::Quad;
using pt::Ray;
using pt::Sampler;
using pt::SolidColor;
using pt::Sphere;
using pt::Vec3;
using pt::operator""_f;
using pt_test::consumed_draws;
using pt_test::make_sampler;
using pt_test::require_color_near;
using pt_test::require_near;

// Wound so that its normal is -z: a ray travelling towards +z meets its front.
[[nodiscard]] Quad facing_camera(Float z, const pt::Material* mat) {
    return Quad(Point3(-1, -1, z), Vec3(0, 2, 0), Vec3(2, 0, 0), mat);
}

// Wound the other way, for a surface met by a ray travelling towards -z.
[[nodiscard]] Quad facing_away(Float z, const pt::Material* mat) {
    return Quad(Point3(-1, -1, z), Vec3(2, 0, 0), Vec3(0, 2, 0), mat);
}

const Ray forward{Point3(0, 0, 0), Vec3(0, 0, 1)};

const ImportanceTargets no_targets;
const std::span<const ConstantMedium> no_media;

const Color background{0.2_f, 0.4_f, 0.6_f};

} // namespace

TEST_CASE("a path that hits nothing returns the background", "[render][integrator]") {
    const HittableList empty_world;
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
    const SolidColor white{Color(1, 1, 1)};
    const DiffuseLight lamp{&white};
    const Quad light = facing_camera(2.0_f, &lamp);

    HittableList world;
    world.add(&light);

    // Depth is checked before anything else, so even a light directly in view
    // contributes nothing once the budget is spent.
    const PathIntegrator integrator{world, no_media, no_targets, background, 0};

    Sampler sampler = make_sampler(112);

    // Black rather than the background: an exhausted path is a path whose
    // remaining contribution is unknown, and guessing the background there would
    // add light that no surface emitted. The bias shows up as glowing edges deep
    // inside a Cornell box.
    require_color_near(integrator.radiance(forward, sampler), Color(0, 0, 0));
}

TEST_CASE("a light ends the path with its emission", "[render][integrator]") {
    const SolidColor bright{Color(4, 3, 2)};
    const DiffuseLight lamp{&bright};
    const Quad light = facing_camera(2.0_f, &lamp);

    HittableList world;
    world.add(&light);

    const PathIntegrator integrator{world, no_media, no_targets, background, 8};

    Sampler sampler = make_sampler(113);
    require_color_near(integrator.radiance(forward, sampler), Color(4, 3, 2));

    // A light absorbs: scatter returns nothing, so the emission is the whole
    // answer and the recursion stops without a further draw.
    REQUIRE(consumed_draws(sampler, 113, 0));
}

TEST_CASE("the back of a light is not a light", "[render][integrator]") {
    const SolidColor bright{Color(4, 3, 2)};
    const DiffuseLight lamp{&bright};
    const Quad turned_away = facing_away(2.0_f, &lamp);

    HittableList world;
    world.add(&turned_away);

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

    HittableList world;
    world.add(&reflector);
    world.add(&light);

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

    HittableList world;
    world.add(&reflector);
    world.add(&light);

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

TEST_CASE("the diffuse estimator weighs a lambertian bounce by one", "[render][integrator]") {
    const SolidColor albedo{Color(0.6_f, 0.6_f, 0.6_f)};
    const Lambertian matte{&albedo};
    const Quad surface = facing_camera(2.0_f, &matte);

    HittableList world;
    world.add(&surface);

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

    HittableList world;
    world.add(&surface);

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
    HittableList world;
    world.add(&light);

    // Dense enough that a free path of two units is never sampled: the mean free
    // path is a fiftieth of a unit.
    const Sphere boundary{Point3(0, 0, 2), 1.0_f, nullptr};
    std::vector<ConstantMedium> media;
    media.emplace_back(&boundary, 50.0_f, &soot);

    const PathIntegrator integrator{world, media, no_targets, Color(0, 0, 0), 8};

    Sampler sampler = make_sampler(122);

    // The scattering event replaces the surface hit, and a black phase function
    // absorbs the path there. Free-flight sampling happens in the integrator
    // rather than inside Hittable::hit precisely so it can compete with the
    // surface on distance like this.
    require_color_near(integrator.radiance(forward, sampler), Color(0, 0, 0));
}

TEST_CASE("a medium behind a surface is not sampled", "[render][integrator]") {
    const SolidColor white{Color(1, 1, 1)};
    const SolidColor black{Color(0, 0, 0)};
    const DiffuseLight lamp{&white};
    const Isotropic soot{&black};

    const Quad light = facing_camera(5.0_f, &lamp);
    HittableList world;
    world.add(&light);

    // The same dense fog, moved behind the light.
    const Sphere boundary{Point3(0, 0, 8), 1.0_f, nullptr};
    std::vector<ConstantMedium> media;
    media.emplace_back(&boundary, 50.0_f, &soot);

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

    HittableList world;
    world.add(&surface);

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
