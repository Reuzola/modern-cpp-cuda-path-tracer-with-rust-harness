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
#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <numbers>

namespace {

using pt::dot;
using pt::HitRecord;
using pt::Interval;
using pt::Point3;
using pt::Ray;
using pt::Sampler;
using pt::Sphere;
using pt::unit_vector;
using pt::Vec3;
using pt::operator""_f;
using pt_test::make_sampler;
using pt_test::require_aabb_near;
using pt_test::require_near;
using pt_test::require_uv_near;
using pt_test::require_vec_near;

const Interval visible{0.0_f, pt::infinity};

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
    // Every primitive agrees on this: the interval is open at both ends, so a
    // surface at exactly the closest distance found so far never displaces the one
    // already recorded.
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

    // The poles: v saturates at 0 and 1. The normal is renormalised, but its
    // rounding can still leave y a hair past +/-1 here; without the clamp acos
    // returns NaN and the texture lookup reads out of bounds.
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

TEST_CASE("a distant ray hits a small sphere at the right distance", "[geometry][sphere]") {
    // At 10^5 units the discriminant written as h*h - a*c collapses to zero in
    // float: the sphere is reported as grazed at its closest point rather than
    // entered, one radius short, with a normal to match. This is the camera-in-a
    // -large-scene case, and nothing in the scene set was far enough away to show it.
    const Sphere unit{Point3(0, 0, 0), 1.0_f, nullptr};
    const Ray far_away{Point3(0, 0, -100'000.0_f), Vec3(0, 0, 1)};

    HitRecord rec;
    REQUIRE(unit.hit(far_away, visible, rec));

    require_near(rec.t, 99'999.0_f, 0.01);
    require_vec_near(rec.normal, Vec3(0, 0, -1));
}

TEST_CASE("a hit is reprojected onto the sphere, within its own margin", "[geometry][sphere]") {
    // The ground sphere of the classic scenes, hit from a few units up. r.at(t)
    // carries t's error, which grows with the radius; the reprojected point keeps
    // only the rounding of centre + radius * normal. The margin the record carries
    // must at least bound how far the point is from the surface - measured in
    // double, where the distance is exact to far below that margin.
    const Point3 center(0, -1000, 0);
    constexpr pt::Float radius = 1000.0_f;
    const Sphere ground{center, radius, nullptr};
    Sampler sampler = make_sampler(4);

    for (int i = 0; i < 256; ++i) {
        const Point3 eye(sampler.next_scalar(-50.0_f, 50.0_f), sampler.next_scalar(1.0_f, 50.0_f), sampler.next_scalar(-50.0_f, 50.0_f));
        // Aimed at a point on the surface, so the ray cannot pass over the horizon.
        const pt::Float x = sampler.next_scalar(-20.0_f, 20.0_f);
        const pt::Float z = sampler.next_scalar(-20.0_f, 20.0_f);
        const Point3 target(x, center.y() + std::sqrt(radius * radius - x * x - z * z), z);

        HitRecord rec;
        REQUIRE(ground.hit(Ray(eye, target - eye), visible, rec));

        const double dx = pt_test::widen(rec.p.x()) - pt_test::widen(center.x());
        const double dy = pt_test::widen(rec.p.y()) - pt_test::widen(center.y());
        const double dz = pt_test::widen(rec.p.z()) - pt_test::widen(center.z());
        const double off_surface = std::abs(std::sqrt(dx * dx + dy * dy + dz * dz) - pt_test::widen(radius));
        REQUIRE(off_surface <= pt_test::widen(rec.spawn_margin));

        // Divided by the radial vector's true length, not by the radius: the normal
        // is unit to a few ulps, where (p - centre) / radius would inherit p's error.
        const double unit_tolerance = 4.0 * pt_test::widen(std::numeric_limits<pt::Float>::epsilon());
        REQUIRE(std::abs(pt_test::widen(rec.normal.length()) - 1.0) <= unit_tolerance);
    }
}

TEST_CASE("the spawn margin follows the radius, never the hit point", "[geometry][sphere]") {
    // What the margin covers - the reprojected point and the next query's
    // c = |oc|^2 - r^2 - errs in proportion to the radius alone. Doubling the radius
    // doubles it exactly, a product by two being exact, and a second hit elsewhere
    // on the same sphere carries the same value.
    const Sphere small{Point3(0, 0, 5), 1.0_f, nullptr};
    const Sphere large{Point3(0, 0, 5), 2.0_f, nullptr};

    HitRecord centre_hit;
    HitRecord large_hit;
    HitRecord off_centre_hit;
    REQUIRE(small.hit(forward, visible, centre_hit));
    REQUIRE(large.hit(forward, visible, large_hit));
    REQUIRE(small.hit(Ray(Point3(0.5_f, 0, 0), Vec3(0, 0, 1)), visible, off_centre_hit));

    REQUIRE(centre_hit.spawn_margin > 0.0_f);
    REQUIRE(large_hit.spawn_margin == 2.0_f * centre_hit.spawn_margin);
    REQUIRE(off_centre_hit.spawn_margin == centre_hit.spawn_margin);
}

TEST_CASE("the cone's density is exact at every distance", "[geometry][sphere]") {
    // 1 - cos(theta_max) written as 1 - sqrt(1 - r^2/d^2) subtracts two numbers that
    // agree in nearly every digit float has once the light is far away: at a thousand
    // radii the density came out seven percent low. The reference is the same
    // quantity in double, in the form that subtracts nothing. The nearest distances
    // close the other end, a shading point almost on the light, where the cone opens
    // to a hemisphere and no test had looked before.
    const double distance_over_radius = GENERATE(1.001, 1.01, 1.1, 3.0, 10.0, 30.0, 100.0, 300.0, 1000.0, 3000.0);

    constexpr pt::Float radius = 0.5_f;
    const auto distance = pt_test::narrow(distance_over_radius * pt_test::widen(radius));
    const Sphere light{Point3(0, 0, distance), radius, nullptr};

    // From the float values the sphere actually holds, so the reference answers the
    // same question the engine was asked.
    const double r = pt_test::widen(radius);
    const double d = pt_test::widen(distance);
    const double x = (r * r) / (d * d);
    const double expected = 1.0 / (2.0 * std::numbers::pi * (x / (1.0 + std::sqrt(1.0 - x))));

    const double actual = pt_test::widen(light.pdf_direction(Point3(0, 0, 0), Vec3(0, 0, 1)));

    // A few ulps of float relative error: the stable form stays near 1e-7 at every
    // distance, while the cancelling one is already past 2e-5 at thirty radii.
    INFO("d/r " << distance_over_radius << ": pdf " << actual << " against " << expected);
    REQUIRE(std::abs(actual / expected - 1.0) < 1.0e-5);
}

TEST_CASE("pdf_direction stays finite with the shading point on the surface", "[geometry][sphere]") {
    // The near end of the cone: a point on the sphere itself, where distance and radius agree
    // only to rounding. It is judged either inside (no cone, density zero) or just outside (a
    // cone opened to a hemisphere, density 1/(2 pi)). Anything else - a NaN from sqrt(1 - x)
    // with x a hair past one, an infinity from a vanishing solid angle - would reach the
    // mixture estimator and stay in the pixel for good.
    const Sphere unit{Point3(0, 0, 0), 1.0_f, nullptr};
    Sampler sampler = make_sampler(6);
    constexpr double hemisphere_density = 1.0 / (2.0 * std::numbers::pi);

    for (int i = 0; i < 256; ++i) {
        const Vec3 n = pt::random_unit_vector(sampler);

        // Into the sphere, from the hemisphere around -n: the chord makes the test hit.
        Vec3 into = pt::random_unit_vector(sampler);
        if (dot(into, n) > 0.0_f) into = -into;

        const pt::Float pdf = unit.pdf_direction(n, into);
        INFO("normal (" << n.x() << ", " << n.y() << ", " << n.z() << "): pdf " << pdf);
        REQUIRE(std::isfinite(pdf));

        const bool judged_inside = pdf == 0.0_f;
        const bool judged_hemisphere = std::abs(pt_test::widen(pdf) / hemisphere_density - 1.0) < 1.0e-2;
        REQUIRE((judged_inside || judged_hemisphere));
    }
}

TEST_CASE("directions sampled toward a distant light land on it", "[geometry][sphere]") {
    // The cone sampled through a cancelling 1 - cos also came out too wide, so a share
    // of the directions aimed at a far light missed it: a sample the pdf then calls
    // impossible. Half a percent missed at three hundred radii, five percent at a
    // thousand, half of them at three thousand. The stable form leaves only rounding
    // at the cone's very edge, which none of these draws hit; one in a thousand is
    // the bound, so the test does not hang on that edge either way.
    const pt::Float distance_over_radius = GENERATE(300.0_f, 1000.0_f, 3000.0_f);

    constexpr pt::Float radius = 0.01_f;
    const Sphere light{Point3(0.3_f, -0.2_f, distance_over_radius * radius), radius, nullptr};
    const Point3 origin(0, 0, 0);

    Sampler sampler = make_sampler(5);
    constexpr int samples = 20'000;
    int misses = 0;

    for (int i = 0; i < samples; ++i) {
        const Vec3 direction = light.sample_direction(origin, sampler);
        HitRecord rec;
        if (!light.hit(Ray(origin, direction), visible, rec)) ++misses;
    }

    INFO("d/r " << distance_over_radius << ": " << misses << " of " << samples << " sampled directions missed");
    REQUIRE(misses <= samples / 1000);
}
