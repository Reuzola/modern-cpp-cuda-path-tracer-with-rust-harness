#include "pt/core/hit_record.hpp"
#include "pt/materials/dielectric.hpp"
#include "pt/materials/diffuse_light.hpp"
#include "pt/materials/isotropic.hpp"
#include "pt/materials/lambertian.hpp"
#include "pt/materials/material.hpp"
#include "pt/materials/metal.hpp"
#include "pt/math/color.hpp"
#include "pt/math/constants.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/sampler.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include "pt/sampling/cosine_pdf.hpp"
#include "pt/sampling/pdf_variant.hpp"
#include "pt/sampling/sphere_pdf.hpp"
#include "pt/textures/solid_color.hpp"
#include "support/probe_texture.hpp"
#include "support/test_support.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <cstdint>
#include <optional>
#include <variant>

namespace {

using pt::as_pdf;
using pt::Color;
using pt::CosinePdf;
using pt::Dielectric;
using pt::DiffuseBounce;
using pt::DiffuseLight;
using pt::dot;
using pt::Float;
using pt::HitRecord;
using pt::Isotropic;
using pt::Lambertian;
using pt::Material;
using pt::Metal;
using pt::Point3;
using pt::Ray;
using pt::Sampler;
using pt::ScatterRecord;
using pt::SolidColor;
using pt::SpecularBounce;
using pt::SpherePdf;
using pt::unit_vector;
using pt::Vec3;
using pt::operator""_f;
using pt_test::make_sampler;
using pt_test::ProbeTexture;
using pt_test::require_color_near;
using pt_test::require_near;
using pt_test::require_vec_near;

/// A surface at the origin whose stored normal already faces the ray, which is
/// the state every material sees: the primitives resolve orientation first.
[[nodiscard]] HitRecord surface(const Vec3& facing_normal, bool front_face = true) {
    HitRecord rec;
    rec.p = Point3(0, 0, 0);
    rec.normal = facing_normal;
    rec.front_face = front_face;
    rec.u = 0.25_f;
    rec.v = 0.75_f;
    rec.t = 1.0_f;
    return rec;
}

/// A material that implements nothing but the pure virtual, to reach the base
/// class's own answers.
class PlainMaterial final : public Material {
public:
    [[nodiscard]] std::optional<ScatterRecord> scatter(const Ray&, const HitRecord&, Sampler&) const override {
        return std::nullopt;
    }
};

// Counts how far a stream advanced, in draws.
[[nodiscard]] int draws_between(Sampler& used, std::uint64_t stream) {
    Sampler counted = make_sampler(stream);
    for (int i = 0; i < 64; ++i) {
        if (used.next_uint32() == counted.next_uint32()) return i;
        static_cast<void>(counted.next_uint32());
    }
    return -1;
}

} // namespace

TEST_CASE("a material emits nothing and carries no density by default", "[materials]") {
    const PlainMaterial plain;
    const HitRecord rec = surface(Vec3(0, 0, -1));
    const Ray r{Point3(0, 0, -1), Vec3(0, 0, 1)};

    // Metal and Dielectric inherit both of these. The zero density is what makes
    // a specular bounce skip the PDF weighting entirely: it is never asked to
    // divide by it.
    require_color_near(plain.emitted(r, rec), Color(0, 0, 0));
    require_near(plain.scattering_pdf(r, rec, r), 0.0_f);
}

TEST_CASE("lambertian scatters into a cosine lobe about the normal", "[materials][lambertian]") {
    const SolidColor albedo{Color(0.4_f, 0.6_f, 0.8_f)};
    const Lambertian material{&albedo};

    const HitRecord rec = surface(Vec3(0, 1, 0));
    Sampler sampler = make_sampler(71);

    const auto scattered = material.scatter(Ray(Point3(0, 5, 0), Vec3(0, -1, 0)), rec, sampler);
    REQUIRE(scattered.has_value());

    require_color_near(scattered->attenuation, Color(0.4_f, 0.6_f, 0.8_f));

    // A diffuse bounce hands back a distribution, not a direction: the integrator
    // mixes it with the light-sampling PDF and only then draws. That is the whole
    // reason ScatterRecord is a variant.
    REQUIRE(std::holds_alternative<DiffuseBounce>(scattered->bounce));
    const DiffuseBounce& bounce = std::get<DiffuseBounce>(scattered->bounce);
    REQUIRE(std::holds_alternative<CosinePdf>(bounce.sampling_pdf));

    // The lobe is built about the surface normal, so straight up carries the
    // maximum density and grazing carries none.
    require_near(as_pdf(bounce.sampling_pdf).value(Vec3(0, 1, 0)), 1.0_f / pt::pi);
    require_near(as_pdf(bounce.sampling_pdf).value(Vec3(1, 0, 0)), 0.0_f);

    // No draw is taken here. Scattering decides the shape; sampling happens later.
    REQUIRE(draws_between(sampler, 71) == 0);
}

TEST_CASE("lambertian's density matches the lobe it samples from", "[materials][lambertian]") {
    const SolidColor albedo{Color(0.5_f, 0.5_f, 0.5_f)};
    const Lambertian material{&albedo};

    const HitRecord rec = surface(Vec3(0, 1, 0));
    const Ray incoming{Point3(0, 5, 0), Vec3(0, -1, 0)};

    Sampler sampler = make_sampler(72);
    const auto scattered = material.scatter(incoming, rec, sampler);
    const DiffuseBounce& bounce = std::get<DiffuseBounce>(scattered->bounce);

    // The BRDF's density and the PDF the material offers to sample it with are
    // the same function. Their ratio is therefore exactly one, which is why a
    // purely diffuse bounce adds no variance of its own - and why the two
    // drifting apart would give a plausible but wrong image rather than an
    // obviously broken one.
    const Vec3 directions[] = {Vec3(0, 1, 0), Vec3(1, 1, 0), Vec3(0.2_f, 0.9_f, -0.3_f), Vec3(3, 4, 0)};

    for (const Vec3& direction : directions) {
        const Ray out{rec.p, direction};
        require_near(material.scattering_pdf(incoming, rec, out), as_pdf(bounce.sampling_pdf).value(direction));
    }

    // 60 degrees off the normal: cos/pi.
    require_near(material.scattering_pdf(incoming, rec, Ray(rec.p, Vec3(std::sqrt(3.0_f), 1, 0))), 0.5_f / pt::pi);

    // Below the surface the density is clamped, not negative. A negative density
    // would come back as negative radiance and no amount of averaging removes it.
    require_near(material.scattering_pdf(incoming, rec, Ray(rec.p, Vec3(0, -1, 0))), 0.0_f);
}

TEST_CASE("lambertian looks its colour up where the ray landed", "[materials][lambertian]") {
    const ProbeTexture probe{Color(0.1_f, 0.2_f, 0.3_f)};
    const Lambertian material{&probe};

    HitRecord rec = surface(Vec3(0, 1, 0));
    rec.p = Point3(4, -5, 6);

    Sampler sampler = make_sampler(73);
    const auto scattered = material.scatter(Ray(Point3(0, 5, 0), Vec3(0, -1, 0)), rec, sampler);

    // All three coordinates are forwarded: uv for image and checker lookups, and
    // the world position for the solid textures that ignore uv entirely.
    REQUIRE(probe.calls() == 1);
    require_near(probe.last_u(), 0.25_f);
    require_near(probe.last_v(), 0.75_f);
    require_vec_near(probe.last_p(), Point3(4, -5, 6));
    require_color_near(scattered->attenuation, Color(0.1_f, 0.2_f, 0.3_f));
}

TEST_CASE("a polished metal reflects exactly", "[materials][metal]") {
    const Metal mirror{Color(0.9_f, 0.9_f, 0.9_f), 0.0_f};
    const HitRecord rec = surface(Vec3(0, 1, 0));

    Sampler sampler = make_sampler(74);
    const Ray incoming{Point3(-1, 1, 0), Vec3(1, -1, 0)};
    const auto scattered = mirror.scatter(incoming, rec, sampler);

    REQUIRE(scattered.has_value());
    require_color_near(scattered->attenuation, Color(0.9_f, 0.9_f, 0.9_f));

    // A specular bounce is a single direction, so it comes back as a ray rather
    // than a distribution and skips the PDF machinery altogether.
    REQUIRE(std::holds_alternative<SpecularBounce>(scattered->bounce));
    const Ray& out = std::get<SpecularBounce>(scattered->bounce).scattered;

    // Mirrored about the normal: the tangential component survives, the normal
    // component flips.
    require_vec_near(unit_vector(out.direction()), unit_vector(Vec3(1, 1, 0)));
    require_vec_near(out.origin(), rec.p);

    // Nothing was drawn: the offset is skipped entirely rather than multiplied
    // by zero, so a mirror leaves the stream where it found it.
    REQUIRE(draws_between(sampler, 74) == 0);
}

TEST_CASE("a specular bounce keeps the ray's place in the shutter", "[materials][metal]") {
    const Metal mirror{Color(1, 1, 1), 0.0_f};
    const HitRecord rec = surface(Vec3(0, 1, 0));

    Sampler sampler = make_sampler(75);
    const auto scattered = mirror.scatter(Ray(Point3(-1, 1, 0), Vec3(1, -1, 0), 0.375_f), rec, sampler);

    // Motion blur is a property of the path, not of one segment: a reflection off
    // a mirror must see the rest of the scene frozen at the same instant, or a
    // moving object appears in two places in the same image.
    require_near(std::get<SpecularBounce>(scattered->bounce).scattered.time(), 0.375_f);
}

TEST_CASE("fuzz is clamped to a usable range", "[materials][metal]") {
    const HitRecord rec = surface(Vec3(0, 1, 0));
    const Ray incoming{Point3(-1, 1, 0), Vec3(1, -1, 0)};

    // Below zero it would subtract from the reflected direction and tilt every
    // highlight in one direction; above one the offset dominates the reflection
    // entirely and most samples end up rejected below the surface.
    const Metal negative{Color(1, 1, 1), -3.0_f};
    Sampler sampler = make_sampler(76);
    const auto scattered = negative.scatter(incoming, rec, sampler);
    require_vec_near(unit_vector(std::get<SpecularBounce>(scattered->bounce).scattered.direction()), unit_vector(Vec3(1, 1, 0)));

    // Clamped to zero, so it behaves as a polished mirror in every respect,
    // including leaving the sampler untouched.
    REQUIRE(draws_between(sampler, 76) == 0);

    const Metal rough{Color(1, 1, 1), 5.0_f};
    Sampler rough_sampler = make_sampler(77);
    int accepted = 0;
    for (int i = 0; i < 256; ++i) {
        if (rough.scatter(incoming, rec, rough_sampler).has_value()) ++accepted;
    }

    // A fuzz of five behaves as a fuzz of one: a hemisphere's worth of spread,
    // with roughly half the offsets pushing the direction below the surface.
    REQUIRE(accepted > 64);
    REQUIRE(accepted < 250);
}

TEST_CASE("a fuzzed reflection below the surface is absorbed", "[materials][metal]") {
    const Metal rough{Color(1, 1, 1), 1.0_f};

    // Grazing incidence: the reflected direction lies almost in the surface, so a
    // full-strength offset frequently pushes it underneath.
    const HitRecord rec = surface(Vec3(0, 1, 0));
    const Ray grazing{Point3(-1, 0.05_f, 0), Vec3(1, -0.05_f, 0)};

    Sampler sampler = make_sampler(78);
    int absorbed = 0;

    for (int i = 0; i < 256; ++i) {
        const auto scattered = rough.scatter(grazing, rec, sampler);
        if (!scattered.has_value()) {
            ++absorbed;
            continue;
        }

        // Whatever comes back has to leave the surface. A ray scattered into the
        // geometry immediately hits its own surface from inside and turns into a
        // black speck.
        const Ray& out = std::get<SpecularBounce>(scattered->bounce).scattered;
        REQUIRE(dot(out.direction(), rec.normal) > 0.0_f);
    }

    REQUIRE(absorbed > 0);
}

TEST_CASE("glass is colourless and always specular", "[materials][dielectric]") {
    const Dielectric glass{1.5_f};
    const HitRecord rec = surface(Vec3(0, 0, -1));

    Sampler sampler = make_sampler(79);
    const auto scattered = glass.scatter(Ray(Point3(0, 0, -1), Vec3(0, 0, 1)), rec, sampler);

    REQUIRE(scattered.has_value());

    // Attenuation is exactly white: this model has no absorption inside the
    // medium, so a thick glass sphere is as bright as a thin one.
    require_color_near(scattered->attenuation, Color(1, 1, 1));
    REQUIRE(std::holds_alternative<SpecularBounce>(scattered->bounce));
}

TEST_CASE("total internal reflection needs no random number", "[materials][dielectric]") {
    const Dielectric glass{1.5_f};

    // Leaving the glass at 45 degrees: front_face is false, so the ratio is the
    // index itself and 1.5 * sin(45) exceeds one - there is no outgoing angle.
    const HitRecord rec = surface(Vec3(0, 0, -1), false);
    const Float diagonal = 1.0_f / std::sqrt(2.0_f);
    const Ray incoming{Point3(-1, 0, -1), Vec3(diagonal, 0, diagonal)};

    Sampler sampler = make_sampler(80);
    const auto scattered = glass.scatter(incoming, rec, sampler);

    const Ray& out = std::get<SpecularBounce>(scattered->bounce).scattered;
    require_vec_near(out.direction(), Vec3(diagonal, 0, -diagonal), 1e-3);

    // The Fresnel coin is only flipped when refraction is possible: the guard is
    // the left operand of a short-circuiting ||. Drawing here anyway would still
    // render correctly, and would shift the RNG stream for every path that grazes
    // a glass surface.
    REQUIRE(draws_between(sampler, 80) == 0);
}

TEST_CASE("four percent reflects at normal incidence", "[materials][dielectric]") {
    const Dielectric glass{1.5_f};
    const HitRecord rec = surface(Vec3(0, 0, -1));
    const Ray incoming{Point3(0, 0, -1), Vec3(0, 0, 1)};

    Sampler sampler = make_sampler(81);
    constexpr int samples = 4096;
    int reflected = 0;

    for (int i = 0; i < samples; ++i) {
        const auto scattered = glass.scatter(incoming, rec, sampler);
        const Ray& out = std::get<SpecularBounce>(scattered->bounce).scattered;

        // Straight through, or straight back.
        if (out.direction().z() < 0.0_f) ++reflected;
    }

    // Schlick's r0 for an index of 1.5 is ((1 - 1/1.5) / (1 + 1/1.5))^2 = 0.04.
    // That number is why a window is a window and not a mirror, and it is the one
    // place the refraction index shows up on its own rather than as a ratio, so a
    // sign or an inversion error moves it visibly.
    const double fraction = static_cast<double>(reflected) / static_cast<double>(samples);
    REQUIRE(fraction > 0.025);
    REQUIRE(fraction < 0.055);
}

TEST_CASE("refraction bends by Snell's law", "[materials][dielectric]") {
    const Dielectric glass{1.5_f};

    // Entering the glass at 45 degrees.
    const HitRecord rec = surface(Vec3(0, 0, -1));
    const Float diagonal = 1.0_f / std::sqrt(2.0_f);
    const Ray incoming{Point3(-1, 0, -1), Vec3(diagonal, 0, diagonal)};

    // sin(out) = sin(45) / 1.5 = 0.4714, so the transmitted direction is
    // (0.4714, 0, 0.8819) - bent towards the normal on the way in.
    const Vec3 expected(0.4714045_f, 0, 0.8819171_f);

    Sampler sampler = make_sampler(82);
    int refracted = 0;

    for (int i = 0; i < 64; ++i) {
        const auto scattered = glass.scatter(incoming, rec, sampler);
        const Ray& out = std::get<SpecularBounce>(scattered->bounce).scattered;

        // Every outgoing direction is a unit vector, whichever branch produced it.
        require_near(out.direction().length(), 1.0_f, 1e-3);

        if (out.direction().z() > 0.0_f) {
            require_vec_near(out.direction(), expected, 1e-3);
            ++refracted;
        }
    }

    // Reflection is the rare branch at this angle, but it does happen; this is
    // about the common one being right.
    REQUIRE(refracted > 55);
}

TEST_CASE("a light emits from its front and scatters nothing", "[materials][diffuse_light]") {
    const ProbeTexture probe{Color(4, 3, 2)};
    const DiffuseLight light{&probe};

    const Ray incoming{Point3(0, 5, 0), Vec3(0, -1, 0)};
    Sampler sampler = make_sampler(83);

    // A light absorbs every path that reaches it: the path ends there and its
    // contribution is the emitted term. Returning a bounce instead would let a
    // path continue through the lamp.
    REQUIRE_FALSE(light.scatter(incoming, surface(Vec3(0, 1, 0)), sampler).has_value());
    REQUIRE(draws_between(sampler, 83) == 0);

    // Emission is above one on purpose - that is what makes it a light rather
    // than a bright surface, and why the film needs tone mapping at all.
    const HitRecord front = surface(Vec3(0, 1, 0));
    require_color_near(light.emitted(incoming, front), Color(4, 3, 2));
    require_near(probe.last_u(), 0.25_f);
    require_near(probe.last_v(), 0.75_f);

    // The back is dark, so a ceiling lamp does not light the space above it and
    // an emissive quad works as a one-sided panel.
    const HitRecord back = surface(Vec3(0, -1, 0), false);
    require_color_near(light.emitted(incoming, back), Color(0, 0, 0));
}

TEST_CASE("an isotropic medium scatters over the whole sphere", "[materials][isotropic]") {
    const ProbeTexture probe{Color(0.7_f, 0.7_f, 0.7_f)};
    const Isotropic phase{&probe};

    const HitRecord rec = surface(Vec3(1, 0, 0));
    const Ray incoming{Point3(0, 0, -1), Vec3(0, 0, 1)};

    Sampler sampler = make_sampler(84);
    const auto scattered = phase.scatter(incoming, rec, sampler);

    REQUIRE(scattered.has_value());
    require_color_near(scattered->attenuation, Color(0.7_f, 0.7_f, 0.7_f));

    const DiffuseBounce& bounce = std::get<DiffuseBounce>(scattered->bounce);
    REQUIRE(std::holds_alternative<SpherePdf>(bounce.sampling_pdf));
    REQUIRE(draws_between(sampler, 84) == 0);

    // Uniform over the sphere: 1 / 4pi in every direction, including backwards.
    // The arbitrary normal the medium wrote into the record is never consulted,
    // which is why it can be arbitrary.
    constexpr Float uniform = 1.0_f / (4.0_f * pt::pi);
    require_near(phase.scattering_pdf(incoming, rec, Ray(rec.p, Vec3(0, 0, 1))), uniform);
    require_near(phase.scattering_pdf(incoming, rec, Ray(rec.p, Vec3(0, 0, -1))), uniform);
    require_near(as_pdf(bounce.sampling_pdf).value(Vec3(0, 1, 0)), uniform);
}
