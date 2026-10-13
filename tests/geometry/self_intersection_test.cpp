#include "pt/core/hit_record.hpp"
#include "pt/core/hittable.hpp"
#include "pt/geometry/instance.hpp"
#include "pt/geometry/quad.hpp"
#include "pt/geometry/sphere.hpp"
#include "pt/geometry/triangle.hpp"
#include "pt/math/constants.hpp"
#include "pt/math/interval.hpp"
#include "pt/math/onb.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/robust.hpp"
#include "pt/math/sampler.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/transform.hpp"
#include "pt/math/vec3.hpp"
#include "support/test_support.hpp"
#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <cmath>
#include <cstdint>
#include <functional>
#include <memory>
#include <utility>

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
using pt::Hittable;
using pt::Instance;
using pt::Onb;
using pt::Quad;
using pt::Transform;
using pt::Triangle;
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

// A planar primitive and a point on it. A plane cannot be met twice by one ray,
// so on either side of it any hit at all is a self-intersection.
struct PlanarCase {
    // Declared first so it is destroyed last: an instance's child must outlive it.
    std::unique_ptr<const Hittable> child;
    std::unique_ptr<const Hittable> surface;
    Point3 target;
    Vec3 outward;
    Float camera_distance{};
};

[[nodiscard]] SelfHits trace_planar_cases(const std::function<PlanarCase(Sampler&)>& generate, std::uint64_t stream) {
    Sampler sampler = make_sampler(stream);
    SelfHits counts;

    for (int i = 0; i < case_count; ++i) {
        const PlanarCase c = generate(sampler);
        const Point3 camera = c.target + c.camera_distance * camera_direction(sampler, c.outward);

        HitRecord rec;
        if (!c.surface->hit(Ray(camera, c.target - camera), visible, rec)) continue;
        ++counts.cases;

        HitRecord self;
        const Ray leaving = rec.spawn_ray(hemisphere_direction(sampler, rec.geometric_normal), 0.0_f);
        if (c.surface->hit(leaving, visible, self)) ++counts.outward;

        const Ray passing = rec.spawn_ray(hemisphere_direction(sampler, -rec.geometric_normal), 0.0_f);
        if (c.surface->hit(passing, visible, self)) ++counts.inward;
    }
    return counts;
}

// A plane through a point within a hundredth of the world origin, tilted off every
// axis, with an orthonormal frame in it.
struct TiltedPlane {
    Point3 anchor;
    Vec3 normal;
    Vec3 a;
    Vec3 b;
};

[[nodiscard]] TiltedPlane tilted_plane_near_origin(Sampler& sampler) {
    const Vec3 normal = pt::random_unit_vector(sampler);
    const Onb frame(normal);
    return TiltedPlane{.anchor = sampler.next_scalar(-0.01_f, 0.01_f) * normal, .normal = normal, .a = frame.u(), .b = frame.v()};
}

// Spans of 10 to 1000 units across the origin. Every coordinate of the surface is
// large, the hit point is not: the plane test rounds terms the size of the
// vertices, and an offset scaled to |p| cannot see them.
[[nodiscard]] PlanarCase large_triangle_near_origin(Sampler& sampler) {
    const TiltedPlane plane = tilted_plane_near_origin(sampler);
    const Float span = std::pow(10.0_f, sampler.next_scalar(1.0_f, 3.0_f));

    const Point3 v0 = plane.anchor - span * plane.a - span * plane.b;
    const Point3 v1 = plane.anchor + 2.0_f * span * plane.a - span * plane.b;
    const Point3 v2 = plane.anchor - span * plane.a + 2.0_f * span * plane.b;
    const Point3 target = plane.anchor + sampler.next_scalar(-0.5_f, 0.5_f) * plane.a + sampler.next_scalar(-0.5_f, 0.5_f) * plane.b;

    return PlanarCase{
        .child = nullptr,
        .surface = std::make_unique<Triangle>(v0, v1, v2, nullptr),
        .target = target,
        .outward = unit_vector(cross(v1 - v0, v2 - v0)),
        .camera_distance = sampler.next_scalar(1.0_f, 50.0_f)};
}

[[nodiscard]] PlanarCase large_quad_near_origin(Sampler& sampler) {
    const TiltedPlane plane = tilted_plane_near_origin(sampler);
    const Float span = std::pow(10.0_f, sampler.next_scalar(1.0_f, 3.0_f));

    const Point3 corner = plane.anchor - span * plane.a - span * plane.b;
    const Point3 target = plane.anchor + sampler.next_scalar(-0.5_f, 0.5_f) * plane.a + sampler.next_scalar(-0.5_f, 0.5_f) * plane.b;

    return PlanarCase{
        .child = nullptr,
        .surface = std::make_unique<Quad>(corner, 2.0_f * span * plane.a, 2.0_f * span * plane.b, nullptr),
        .target = target,
        .outward = unit_vector(cross(plane.a, plane.b)),
        .camera_distance = sampler.next_scalar(1.0_f, 50.0_f)};
}

// Mesh-scale triangles - edges of 5 mm to 20 cm - seen from 5 to 40 units: the
// camera's t is long against the triangle, so r.at(t) errs by more than the
// triangle's own scale.
[[nodiscard]] PlanarCase mesh_triangle(Sampler& sampler) {
    const Point3 center(sampler.next_scalar(-5.0_f, 5.0_f), sampler.next_scalar(-5.0_f, 5.0_f), sampler.next_scalar(-5.0_f, 5.0_f));
    const Float edge = sampler.next_scalar(0.005_f, 0.2_f);

    const Point3 v0 = center + edge * pt::random_unit_vector(sampler);
    const Point3 v1 = center + edge * pt::random_unit_vector(sampler);
    const Point3 v2 = center + edge * pt::random_unit_vector(sampler);
    const Float b1 = sampler.next_scalar(0.05_f, 0.45_f);
    const Float b2 = sampler.next_scalar(0.05_f, 0.45_f);

    return PlanarCase{
        .child = nullptr,
        .surface = std::make_unique<Triangle>(v0, v1, v2, nullptr),
        .target = (1.0_f - b1 - b2) * v0 + b1 * v1 + b2 * v2,
        .outward = unit_vector(cross(v1 - v0, v2 - v0)),
        .camera_distance = sampler.next_scalar(5.0_f, 40.0_f)};
}

// The same scale with aspect ratios down to 1:1000, the shape a decimated scan
// produces. The determinant shrinks with the area, so every error in the test is
// amplified by the condition number - no fixed offset keeps up with that.
[[nodiscard]] PlanarCase sliver_triangle(Sampler& sampler) {
    const Point3 center(sampler.next_scalar(-5.0_f, 5.0_f), sampler.next_scalar(-5.0_f, 5.0_f), sampler.next_scalar(-5.0_f, 5.0_f));
    const Float edge = sampler.next_scalar(0.005_f, 0.2_f);
    const Float aspect = std::pow(10.0_f, sampler.next_scalar(-3.0_f, 0.0_f));

    const Vec3 along = pt::random_unit_vector(sampler);
    const Vec3 across = unit_vector(cross(along, pt::random_unit_vector(sampler)));

    const Point3 v0 = center - edge * along;
    const Point3 v1 = center + edge * along;
    const Point3 v2 = center + sampler.next_scalar(-0.8_f, 0.8_f) * edge * along + aspect * edge * across;
    const Float b1 = sampler.next_scalar(0.05_f, 0.45_f);
    const Float b2 = sampler.next_scalar(0.05_f, 0.45_f);

    return PlanarCase{
        .child = nullptr,
        .surface = std::make_unique<Triangle>(v0, v1, v2, nullptr),
        .target = (1.0_f - b1 - b2) * v0 + b1 * v1 + b2 * v2,
        .outward = unit_vector(cross(v1 - v0, v2 - v0)),
        .camera_distance = sampler.next_scalar(5.0_f, 40.0_f)};
}

// A ten-unit surface whose own coordinates sit 100 to 1000 units from its local
// origin, rotated and translated so that the world sees it near the world origin.
// Every term the transform rounds is large and they cancel to a small point: the
// instance's arithmetic, not the primitive's, is what the offset must clear.
[[nodiscard]] PlanarCase instanced_near_origin(Sampler& sampler, bool quad) {
    const TiltedPlane plane = tilted_plane_near_origin(sampler);
    const Float distance = std::pow(10.0_f, sampler.next_scalar(2.0_f, 3.0_f));
    const Point3 local_centre = distance * pt::random_unit_vector(sampler);
    constexpr Float half = 5.0_f;

    const Point3 corner = local_centre - half * plane.a - half * plane.b;
    std::unique_ptr<const Hittable> child;
    Vec3 local_normal;
    if (quad) {
        child = std::make_unique<Quad>(corner, 2.0_f * half * plane.a, 2.0_f * half * plane.b, nullptr);
        local_normal = unit_vector(cross(plane.a, plane.b));
    } else {
        const Point3 v1 = corner + 3.0_f * half * plane.a;
        const Point3 v2 = corner + 3.0_f * half * plane.b;
        child = std::make_unique<Triangle>(corner, v1, v2, nullptr);
        local_normal = unit_vector(cross(v1 - corner, v2 - corner));
    }

    const Transform rotation = Transform::rotation_y(sampler.next_scalar(0.0_f, 360.0_f)) * Transform::rotation_x(sampler.next_scalar(0.0_f, 360.0_f));
    const Transform placement = Transform::translation(-rotation.apply_point(local_centre)) * rotation;
    const Point3 local_target = local_centre + sampler.next_scalar(-0.5_f, 0.5_f) * plane.a + sampler.next_scalar(-0.5_f, 0.5_f) * plane.b;

    auto instance = std::make_unique<Instance>(child.get(), placement);
    return PlanarCase{.child = std::move(child), .surface = std::move(instance), .target = placement.apply_point(local_target), .outward = unit_vector(rotation.apply_vector(local_normal)), .camera_distance = sampler.next_scalar(1.0_f, 50.0_f)};
}

[[nodiscard]] PlanarCase instanced_triangle(Sampler& sampler) { return instanced_near_origin(sampler, false); }

[[nodiscard]] PlanarCase instanced_quad(Sampler& sampler) { return instanced_near_origin(sampler, true); }

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

TEST_CASE("a ray spawned from a planar primitive never meets it again", "[geometry][self_intersection]") {
    struct Scenario {
        const char* name;
        PlanarCase (*generate)(Sampler&);
        std::uint64_t stream;
    };

    const Scenario scenario = GENERATE(Scenario{"large triangle near the origin", large_triangle_near_origin, 404},
                                       Scenario{"large quad near the origin", large_quad_near_origin, 405},
                                       Scenario{"mesh-scale triangle", mesh_triangle, 406},
                                       Scenario{"sliver triangle", sliver_triangle, 407},
                                       Scenario{"instanced triangle far from its own origin", instanced_triangle, 408},
                                       Scenario{"instanced quad far from its own origin", instanced_quad, 409});

    const SelfHits counts = trace_planar_cases(scenario.generate, scenario.stream);

    INFO(scenario.name << ": cases " << counts.cases << ", outward self-hits " << counts.outward << ", inward self-hits " << counts.inward);
    REQUIRE(counts.cases > case_count * 99 / 100);
    CHECK(counts.outward == 0);
    CHECK(counts.inward == 0);
}
