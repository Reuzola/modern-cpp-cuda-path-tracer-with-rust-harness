#include "pt/core/hit_record.hpp"
#include "pt/geometry/sphere.hpp"
#include "pt/math/constants.hpp"
#include "pt/math/interval.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/robust.hpp"
#include "pt/math/sampler.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include "support/test_support.hpp"
#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <cstdint>
#include <functional>

namespace {

using pt::Float;
using pt::HitRecord;
using pt::Interval;
using pt::Point3;
using pt::Ray;
using pt::Sampler;
using pt::Sphere;
using pt::Vec3;
using pt::operator""_f;
using pt_test::make_sampler;

const Interval visible{0.0_f, pt::infinity};

// Enough to see a failure rate of a few in ten thousand. The zero below was also
// checked offline at two million cases per scenario; this count keeps the suite fast.
constexpr int case_count = 20'000;

// Camera rays closer to tangent than this are left out. Near grazing incidence float
// cannot resolve the chord through a sphere: the root lands at the tangent point and
// front_face becomes a coin flip. That is a limit of the discriminant, not of
// spawning, and in a render it covers a small fraction of one pixel.
constexpr Float min_camera_cosine = 0.05_f;

// Inward rays this close to tangent cross a chord too short to tell from a self-hit.
constexpr Float min_inward_cosine = 1e-3_f;

// Uniform over the hemisphere around `normal`, grazing directions included: they
// are where an offset is thinnest.
[[nodiscard]] Vec3 hemisphere_direction(Sampler& sampler, const Vec3& normal) {
    const Vec3 direction = pt::random_unit_vector(sampler);
    return dot(direction, normal) < 0.0_f ? -direction : direction;
}

[[nodiscard]] Vec3 camera_direction(Sampler& sampler, const Vec3& normal) {
    while (true) {
        const Vec3 direction = hemisphere_direction(sampler, normal);
        if (dot(direction, normal) >= min_camera_cosine) return direction;
    }
}

// One generated configuration: a sphere, a point on it, the true outward normal
// there, and how far away the camera stands.
struct SphereCase {
    Point3 center;
    Float radius{};
    Vec3 outward;
    Float camera_distance{};
};

struct SelfHits {
    int cases{};
    int outward{};
    int inward{};
};

// Hits the sphere from the camera, then spawns one ray to each side of the hit.
// Outward: a ray leaving a convex surface cannot meet it again, so any hit is a
// self-intersection. Inward: the ray must cross the whole chord to the far side;
// stopping short of half of it means it met the surface it was spawned on.
[[nodiscard]] SelfHits trace_cases(const std::function<SphereCase(Sampler&)>& generate, std::uint64_t stream) {
    Sampler sampler = make_sampler(stream);
    SelfHits counts;

    for (int i = 0; i < case_count; ++i) {
        const SphereCase c = generate(sampler);
        const Sphere sphere(c.center, c.radius, nullptr);
        const Point3 target = c.center + c.radius * c.outward;
        const Point3 camera = target + c.camera_distance * camera_direction(sampler, c.outward);

        HitRecord rec;
        // A far camera aimed at a small sphere can miss it by a rounding; such a
        // case says nothing about spawning and is skipped.
        if (!sphere.hit(Ray(camera, target - camera), visible, rec)) continue;
        ++counts.cases;

        HitRecord self;
        const Ray leaving = rec.spawn_ray(hemisphere_direction(sampler, rec.geometric_normal), 0.0_f);
        if (sphere.hit(leaving, visible, self)) ++counts.outward;

        const Vec3 inward = hemisphere_direction(sampler, -rec.geometric_normal);
        const Float cosine = dot(unit_vector(inward), -rec.geometric_normal);
        if (cosine < min_inward_cosine) continue;

        const Float chord = 2.0_f * c.radius * cosine;
        const Ray entering = rec.spawn_ray(inward, 0.0_f);
        if (!sphere.hit(entering, visible, self) || self.t * inward.length() < 0.5_f * chord) ++counts.inward;
    }
    return counts;
}

// Radii from 10 to 1000, hit points within half a unit of the world origin. This
// is the ground sphere of the classic scenes: |p| is tiny, the radius is not, and
// an offset scaled to |p| alone is an order of magnitude too small.
[[nodiscard]] SphereCase large_sphere_near_origin(Sampler& sampler) {
    const Float radius = std::pow(10.0_f, sampler.next_scalar(1.0_f, 3.0_f));
    const Vec3 outward = pt::random_unit_vector(sampler);
    const Point3 hit(sampler.next_scalar(-0.5_f, 0.5_f), sampler.next_scalar(-0.5_f, 0.5_f), sampler.next_scalar(-0.5_f, 0.5_f));

    return SphereCase{.center = hit - radius * outward, .radius = radius, .outward = outward, .camera_distance = sampler.next_scalar(1.0_f, 50.0_f)};
}

// Ordinary spheres seen from up to a thousand units away: t is long, so r.at(t)
// carries an error far larger than the sphere's own scale.
[[nodiscard]] SphereCase small_sphere_far_camera(Sampler& sampler) {
    const Point3 center(sampler.next_scalar(-10.0_f, 10.0_f), sampler.next_scalar(-10.0_f, 10.0_f), sampler.next_scalar(-10.0_f, 10.0_f));

    return SphereCase{.center = center, .radius = sampler.next_scalar(0.1_f, 1.0_f), .outward = pt::random_unit_vector(sampler), .camera_distance = std::pow(10.0_f, sampler.next_scalar(0.0_f, 3.0_f))};
}

} // namespace

TEST_CASE("a ray spawned near a large sphere never meets it again", "[geometry][self_intersection]") {
    const SelfHits counts = trace_cases(large_sphere_near_origin, 401);

    INFO("cases " << counts.cases << ", outward self-hits " << counts.outward << ", inward self-hits " << counts.inward);
    REQUIRE(counts.cases > case_count * 99 / 100);
    CHECK(counts.outward == 0);
    CHECK(counts.inward == 0);
}

TEST_CASE("a ray spawned from a small sphere seen from afar never meets it again", "[geometry][self_intersection]") {
    const SelfHits counts = trace_cases(small_sphere_far_camera, 402);

    INFO("cases " << counts.cases << ", outward self-hits " << counts.outward << ", inward self-hits " << counts.inward);
    REQUIRE(counts.cases > case_count * 99 / 100);
    CHECK(counts.outward == 0);
    CHECK(counts.inward == 0);
}

TEST_CASE("near a large sphere the ulp offset alone is not enough", "[geometry][self_intersection]") {
    // Keeps the margin from being dead weight. The reprojected point is the best a
    // float can do, yet spawning from it with the bare ulp offset still re-hits the
    // sphere: the next query's c = |oc|^2 - r^2 errs by a distance proportional to
    // the radius, which no offset scaled to |p| can see.
    Sampler sampler = make_sampler(403);
    int self_hits = 0;

    for (int i = 0; i < case_count; ++i) {
        const SphereCase c = large_sphere_near_origin(sampler);
        const Sphere sphere(c.center, c.radius, nullptr);
        const Point3 target = c.center + c.radius * c.outward;
        const Point3 camera = target + c.camera_distance * camera_direction(sampler, c.outward);

        HitRecord rec;
        if (!sphere.hit(Ray(camera, target - camera), visible, rec)) continue;

        const Vec3 direction = hemisphere_direction(sampler, rec.geometric_normal);
        const Ray bare(pt::offset_ray_origin(rec.p, rec.geometric_normal), direction);

        HitRecord self;
        if (sphere.hit(bare, visible, self)) ++self_hits;
    }

    REQUIRE(self_hits > 0);
}
