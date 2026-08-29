#include "pt/core/hit_record.hpp"
#include "pt/geometry/sphere.hpp"
#include "pt/math/aabb.hpp"
#include "pt/math/constants.hpp"
#include "pt/math/interval.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/sampler.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include "support/test_support.hpp"
#include <catch2/catch_test_macros.hpp>

namespace {

using pt::HitRecord;
using pt::Interval;
using pt::Point3;
using pt::Ray;
using pt::Sampler;
using pt::Sphere;
using pt::Vec3;
using pt::dot;
using pt::unit_vector;
using pt::operator""_f;
using pt_test::make_sampler;
using pt_test::require_aabb_near;
using pt_test::require_near;
using pt_test::require_uv_near;
using pt_test::require_vec_near;

const Interval visible{0.001_f, pt::infinity};

// Centred ahead of the origin at distance 5, radius 1: the near root is at 4 and
// the far one at 6, which keeps every expectation below a round number.
const Sphere ahead{Point3(0, 0, 5), 1.0_f, nullptr};

const Ray forward{Point3(0, 0, 0), Vec3(0, 0, 1)};

} // namespace

TEST_CASE("a ray through the centre stops at the near surface", "[geometry][sphere]") {
    HitRecord rec;
    REQUIRE(ahead.hit(forward, visible, rec));

    require_near(rec.t, 4.0_f);
    require_vec_near(rec.p, Point3(0, 0, 4));

    // The outward normal points back along the ray, so it is kept as it is and
    // the hit counts as a front face.
    require_vec_near(rec.normal, Vec3(0, 0, -1));
    REQUIRE(rec.front_face);
    REQUIRE(rec.mat == nullptr);
}

TEST_CASE("a ray starting inside leaves through the far surface", "[geometry][sphere]") {
    const Ray inside{Point3(0, 0, 5), Vec3(0, 0, 1)};

    HitRecord rec;
    REQUIRE(ahead.hit(inside, visible, rec));

    // The near root is behind the origin, so the second root is taken. This is
    // the path a refracted ray takes on its way out of a glass sphere.
    require_near(rec.t, 1.0_f);
    require_vec_near(rec.normal, Vec3(0, 0, -1));
    REQUIRE_FALSE(rec.front_face);
}

TEST_CASE("the sphere's interval test is strict", "[geometry][sphere]") {
    HitRecord rec;

    // Exactly at the upper bound: rejected, and the far root is out of range too.
    // Quad accepts the same distance, because it tests with contains() rather
    // than surrounds() - which is what decides the winner between two coincident
    // surfaces, and what makes that decision differ by primitive type.
    REQUIRE_FALSE(ahead.hit(forward, Interval(0.001_f, 4.0_f), rec));
    REQUIRE(ahead.hit(forward, Interval(0.001_f, 4.001_f), rec));

    // Exactly at the lower bound the near root is rejected, and the search falls
    // through to the far one rather than reporting a miss.
    REQUIRE(ahead.hit(forward, Interval(4.0_f, pt::infinity), rec));
    require_near(rec.t, 6.0_f);
    REQUIRE_FALSE(rec.front_face);
}

TEST_CASE("a tangent ray counts as a hit", "[geometry][sphere]") {
    // Grazes the sphere at x = 1: the discriminant is exactly zero.
    const Ray tangent{Point3(1, 0, 0), Vec3(0, 0, 1)};

    HitRecord rec;

    // The rejection is `discriminant < 0`, not `<= 0`. A tangent ray is a
    // measure-zero case, but making it a miss would open a one-sample-wide seam
    // along every silhouette where a shadow ray grazes an occluder.
    REQUIRE(ahead.hit(tangent, visible, rec));
    require_near(rec.t, 5.0_f);
    require_vec_near(rec.p, Point3(1, 0, 5));
}

TEST_CASE("t is measured in units of the direction, not in world distance", "[geometry][sphere]") {
    const Ray doubled{Point3(0, 0, 0), Vec3(0, 0, 2)};

    HitRecord rec;
    REQUIRE(ahead.hit(doubled, visible, rec));

    // Half the t for twice the direction, and the same point in space. Instance
    // depends on exactly this: it transforms the direction without renormalising
    // so that ray_t passes through unscaled and rec.t needs no correction.
    require_near(rec.t, 2.0_f);
    require_vec_near(rec.p, Point3(0, 0, 4));
}

TEST_CASE("a negative radius collapses to a point", "[geometry][sphere]") {
    const Sphere degenerate{Point3(0, 0, 5), -3.0_f, nullptr};

    // Clamped in the constructor rather than left to the intersection maths: a
    // negative radius squares away and would otherwise produce a normal pointing
    // inwards - a sphere lit from the wrong side, with no error anywhere.
    const pt::Aabb box = degenerate.bounding_box();
    require_aabb_near(box, Point3(0, 0, 5), Point3(0, 0, 5), 1e-3);
    REQUIRE(box.x.size() < 0.001_f);
}

TEST_CASE("uv follows the spherical parameterisation, poles included", "[geometry][sphere]") {
    const Sphere unit{Point3(0, 0, 0), 1.0_f, nullptr};
    HitRecord rec;

    // +x is the middle of the u range and the equator.
    REQUIRE(unit.hit(Ray(Point3(5, 0, 0), Vec3(-1, 0, 0)), visible, rec));
    require_uv_near(rec.u, rec.v, 0.5_f, 0.5_f);

    // +z is a quarter turn back from there.
    REQUIRE(unit.hit(Ray(Point3(0, 0, 5), Vec3(0, 0, -1)), visible, rec));
    require_uv_near(rec.u, rec.v, 0.25_f, 0.5_f);

    // The poles: v saturates at 0 and 1. The normal handed to acos is
    // (p - centre) / radius rather than a renormalised vector, so cancellation
    // can push it a hair past +/-1 here; without the clamp acos returns NaN and
    // the texture lookup reads out of bounds.
    REQUIRE(unit.hit(Ray(Point3(0, 5, 0), Vec3(0, -1, 0)), visible, rec));
    require_uv_near(rec.u, rec.v, 0.5_f, 1.0_f);

    REQUIRE(unit.hit(Ray(Point3(0, -5, 0), Vec3(0, 1, 0)), visible, rec));
    require_uv_near(rec.u, rec.v, 0.5_f, 0.0_f);
}

TEST_CASE("a moving sphere is where the ray's time says it is", "[geometry][sphere]") {
    const Sphere moving{Point3(0, 0, 5), Point3(4, 0, 5), 1.0_f, nullptr};

    HitRecord rec;

    // At shutter open it is still at its first centre.
    REQUIRE(moving.hit(Ray(Point3(0, 0, 0), Vec3(0, 0, 1), 0.0_f), visible, rec));
    require_near(rec.t, 4.0_f);

    // Halfway through the exposure it has travelled half the segment, so the ray
    // that hit dead centre at t=0 now misses entirely.
    REQUIRE_FALSE(moving.hit(Ray(Point3(0, 0, 0), Vec3(0, 0, 1), 0.5_f), visible, rec));
    REQUIRE(moving.hit(Ray(Point3(2, 0, 0), Vec3(0, 0, 1), 0.5_f), visible, rec));

    // The bounds cover the whole sweep, not one end of it: the BVH is built once
    // for the entire shutter interval.
    require_aabb_near(moving.bounding_box(), Point3(-1, -1, 4), Point3(5, 1, 6));
}

TEST_CASE("pdf_direction is the reciprocal of the visible solid angle", "[geometry][sphere]") {
    const Point3 origin(0, 0, 0);

    // The sphere subtends a cone of half-angle asin(r / d). Its solid angle is
    // 2*pi*(1 - cos(theta_max)) = 2*pi*(1 - sqrt(1 - 1/25)), so a uniform density
    // over that cone is 1/0.126946 = 7.8773.
    require_near(ahead.pdf_direction(origin, Vec3(0, 0, 1)), 7.8773_f, 1e-3);

    // Uniform over the cone means every direction that hits carries the same
    // density - the value depends on the geometry, not on where inside the disc
    // the ray landed, and not on how long the direction vector is.
    require_near(ahead.pdf_direction(origin, Vec3(0.5_f, 0, 5)), 7.8773_f, 1e-3);
    require_near(ahead.pdf_direction(origin, Vec3(0, 0, 3)), 7.8773_f, 1e-3);
}

TEST_CASE("pdf_direction is zero where the cone does not exist", "[geometry][sphere]") {
    // Nothing in that direction: the mixture PDF must contribute nothing rather
    // than a density for a light the shading point cannot see.
    require_near(ahead.pdf_direction(Point3(0, 0, 0), Vec3(0, 1, 0)), 0.0_f);

    // From inside the sphere there is no bounding cone at all. Returning zero
    // rather than a NaN from sqrt of a negative number is what keeps a camera
    // placed inside a light source from poisoning the whole image.
    require_near(ahead.pdf_direction(Point3(0, 0, 5), Vec3(0, 0, 1)), 0.0_f);
}

TEST_CASE("sample_direction stays inside the cone and draws two scalars", "[geometry][sphere]") {
    const Point3 origin(0, 0, 0);
    Sampler sampler = make_sampler(1);

    // cos of the cone's half-angle for r = 1 at distance 5.
    constexpr pt::Float cos_theta_max = 0.9797958_f;

    for (int i = 0; i < 64; ++i) {
        const Vec3 direction = ahead.sample_direction(origin, sampler);
        REQUIRE(dot(unit_vector(direction), Vec3(0, 0, 1)) >= cos_theta_max - 1e-4_f);

        // Every direction the sampler produces must be one the PDF gives a
        // density for. A sample outside the support divides by zero in the
        // mixture PDF and shows up as a fireflies-only bug at high sample counts.
        REQUIRE(ahead.pdf_direction(origin, direction) > 0.0_f);
    }

    // Two draws, in the order the sampler sees them. The RNG stream is part of
    // what makes a render reproducible, so a third draw here - or the same two
    // in the other order - changes every image that uses light sampling.
    Sampler used = make_sampler(2);
    Sampler counted = make_sampler(2);
    static_cast<void>(ahead.sample_direction(origin, used));
    static_cast<void>(counted.next_scalar());
    static_cast<void>(counted.next_scalar());
    REQUIRE(used.next_uint32() == counted.next_uint32());
}

TEST_CASE("sampling a moving sphere aims at its shutter-open centre", "[geometry][sphere]") {
    const Sphere moving{Point3(0, 0, 5), Point3(20, 0, 5), 1.0_f, nullptr};
    const Point3 origin(0, 0, 0);

    Sampler sampler = make_sampler(3);
    const Vec3 direction = moving.sample_direction(origin, sampler);

    HitRecord rec;

    // Importance sampling ignores the shutter: the cone is built around the
    // centre at time zero. A light that moves far during the exposure is
    // therefore sampled towards where it started, and the mixture PDF pays for it
    // in variance, not in bias - the direction still gets its correct density.
    REQUIRE(moving.hit(Ray(origin, direction, 0.0_f), visible, rec));
    REQUIRE_FALSE(moving.hit(Ray(origin, direction, 1.0_f), visible, rec));
}
