#include "pt/core/hit_record.hpp"
#include "pt/geometry/quad.hpp"
#include "pt/math/constants.hpp"
#include "pt/math/interval.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include "support/test_support.hpp"
#include <array>
#include <catch2/catch_test_macros.hpp>

namespace {

using pt::HitRecord;
using pt::Point3;
using pt::Ray;
using pt::Vec3;
using pt::operator""_f;
using pt::Float;
using pt::infinity;
using pt::Interval;
using pt::Quad;
using pt::Sampler;
using pt_test::make_sampler;
using pt_test::require_vec_near;

// Coordinate scales the engine renders at: a unit prop, a Cornell shell, and two
// scales where a float coordinate is coarser than the surface detail on it.
constexpr std::array<Float, 4> scales{1.0_f, 555.0_f, 10'000.0_f, 100'000.0_f};

// The surface under test: a quad tilted off every axis, so both the hit point and
// the plane test carry rounding error in all three components. An axis aligned
// quad at a round coordinate is arithmetically exact and proves nothing.
constexpr Vec3 surface_u(2, 0.3_f, 0.7_f);
constexpr Vec3 surface_v(0.3_f, 2, -0.4_f);
constexpr int sweep = 8;

[[nodiscard]] Point3 surface_corner(Float scale) {
    return Point3(scale - 1, scale - 1, scale);
}

[[nodiscard]] Quad tilted_surface(Float scale) {
    return Quad(surface_corner(scale), surface_u, surface_v, nullptr);
}

// Hits the surface the way a camera does - unit direction, long t - aiming at
// (a, b) in the quad's own basis, so the hit point carries the rounding of
// origin + t * direction rather than an exact coordinate.
[[nodiscard]] HitRecord hit_at(const Quad& surface, Float scale, Float a, Float b) {
    const Point3 eye(0, 0, 0);
    const Point3 target = surface_corner(scale) + a * surface_u + b * surface_v;

    HitRecord rec;
    REQUIRE(surface.hit(Ray(eye, unit_vector(target - eye)), Interval(0.0_f, infinity), rec));
    return rec;
}

// A direction leaving on the outward side of the hit. A plane cannot be hit from
// the side you are walking away from, so any hit from one of these means the
// origin sits behind its own surface. That is what acne is.
[[nodiscard]] Vec3 outward_direction(Sampler& sampler, const Vec3& normal) {
    const Vec3 direction = random_unit_vector(sampler);
    return dot(direction, normal) < 0.0_f ? -direction : direction;
}

// Travelling towards +z, so a normal pointing back at -z faces it.
const Ray forward{Point3(0, 0, 0), Vec3(0, 0, 1)};

} // namespace

TEST_CASE("a fresh record carries no material and no distance", "[core][hit_record]") {
    const HitRecord rec;

    // The integrator dereferences rec.mat unconditionally after a hit, so the
    // default has to be the value that crashes loudly rather than one that
    // happens to point at the previous surface.
    REQUIRE(rec.mat == nullptr);
    REQUIRE(rec.t == 0.0_f);
    REQUIRE(rec.u == 0.0_f);
    REQUIRE(rec.v == 0.0_f);
    REQUIRE_FALSE(rec.front_face);
}

TEST_CASE("the stored normal always faces the incoming ray", "[core][hit_record]") {
    HitRecord rec;

    rec.set_face_normal(forward, Vec3(0, 0, -1));
    REQUIRE(rec.front_face);
    require_vec_near(rec.normal, Vec3(0, 0, -1));

    // Hitting the far side of a surface: the outward normal points away from the
    // ray, so it is flipped and front_face records that we are inside. Dielectric
    // reads exactly this flag to decide which way the refraction index ratio goes.
    rec.set_face_normal(forward, Vec3(0, 0, 1));
    REQUIRE_FALSE(rec.front_face);
    require_vec_near(rec.normal, Vec3(0, 0, -1));
}

TEST_CASE("a grazing hit counts as a back face", "[core][hit_record]") {
    HitRecord rec;

    // The test is `dot < 0`, not `<= 0`. At exactly zero the ray runs along the
    // surface and neither answer is more correct; the choice is fixed here so
    // that a later `<=` shows up as a failing test rather than as a handful of
    // flipped pixels along a silhouette.
    rec.set_face_normal(forward, Vec3(1, 0, 0));
    REQUIRE_FALSE(rec.front_face);
    require_vec_near(rec.normal, Vec3(-1, 0, 0));
}

TEST_CASE("orientation follows the geometric normal, the shading normal is stored", "[core][hit_record]") {
    HitRecord rec;

    // Near a silhouette an interpolated vertex normal can face the ray even where
    // the triangle itself does not. The geometric normal decides front_face, and
    // the shading normal is flipped to match it - not the other way round.
    const Vec3 geometric(0, 0, 1);        // away from the ray: a back face
    const Vec3 shading(0, 0.6_f, -0.8_f); // towards the ray

    rec.set_face_normal(forward, geometric, shading);

    REQUIRE_FALSE(rec.front_face);
    require_vec_near(rec.normal, Vec3(0, -0.6_f, 0.8_f));

    // The two-argument overload is the same call with the normals equal.
    HitRecord flat;
    flat.set_face_normal(forward, geometric);
    HitRecord explicitly_flat;
    explicitly_flat.set_face_normal(forward, geometric, geometric);

    REQUIRE(flat.front_face == explicitly_flat.front_face);
    require_vec_near(flat.normal, explicitly_flat.normal);
}

TEST_CASE("both orientations resolve at compile time", "[core][hit_record]") {
    // The guard in the header only walks the front-face branch. A back-face path
    // that quietly stopped being constexpr - a branch on a <cmath> call, say -
    // would slip past it.
    STATIC_REQUIRE([] {
        HitRecord rec;
        rec.set_face_normal(Ray(Point3(0, 0, 0), Vec3(0, 0, 1)), Vec3(0, 0, 1));
        return !rec.front_face && rec.normal.z() == -1.0_f;
    }());
}

TEST_CASE("the geometric normal always faces the ray, the shading one need not", "[core][hit_record]") {
    HitRecord rec;

    const Vec3 geometric(0, 0, 1);        // away from the ray: a back face
    const Vec3 shading(0, 0.6_f, -0.8_f); // towards the ray

    rec.set_face_normal(forward, geometric, shading);

    // Both are flipped by one sign, decided by the geometric normal. That leaves
    // the geometric normal facing the ray and, here, the shading normal pointing
    // away from it: near a silhouette the two disagree by more than a right angle.
    // Offsetting along the stored shading normal would push the spawn point into
    // the surface, which is why spawn_ray reads the geometric one.
    REQUIRE_FALSE(rec.front_face);
    require_vec_near(rec.geometric_normal, Vec3(0, 0, -1));
    REQUIRE(dot(rec.geometric_normal, forward.direction()) < 0.0_f);
    REQUIRE(dot(rec.normal, forward.direction()) > 0.0_f);

    // The two-argument overload feeds the same vector to both.
    HitRecord flat;
    flat.set_face_normal(forward, Vec3(0, 0, -1));
    require_vec_near(flat.geometric_normal, flat.normal);
}

TEST_CASE("a spawned ray leaves on the side its direction points to", "[core][hit_record]") {
    HitRecord rec;
    rec.p = Point3(555.0_f, -555.0_f, 555.0_f);
    rec.geometric_normal = Vec3(0, 0, 1);

    const Ray reflected = rec.spawn_ray(Vec3(0.2_f, 0.1_f, 1), 0.25_f);
    REQUIRE(reflected.origin().z() > rec.p.z());

    // Transmission leaves through the surface and the origin has to follow it.
    // This sign is invisible in a Cornell box and fatal in a glass ball.
    const Ray refracted = rec.spawn_ray(Vec3(0.2_f, 0.1_f, -1), 0.25_f);
    REQUIRE(refracted.origin().z() < rec.p.z());

    // Only the normal's axis moves: lateral drift would slide the origin across
    // the surface it just left. And the ray keeps its time for motion blur.
    REQUIRE(reflected.origin().x() == rec.p.x());
    REQUIRE(reflected.origin().y() == rec.p.y());
    REQUIRE(reflected.time() == 0.25_f);
}

TEST_CASE("a volume interaction spawns from its own sample point", "[core][hit_record]") {
    // ConstantMedium leaves the geometric normal zero: there is no surface to
    // escape, and moving the point would displace the scattering event itself.
    for (const Float scale : scales) {
        HitRecord rec;
        rec.p = Point3(scale, -scale, scale);
        rec.geometric_normal = Vec3();

        const Ray spawned = rec.spawn_ray(Vec3(0, 1, 0), 0.0_f);

        REQUIRE(spawned.origin().x() == rec.p.x());
        REQUIRE(spawned.origin().y() == rec.p.y());
        REQUIRE(spawned.origin().z() == rec.p.z());
    }
}

TEST_CASE("a spawned ray does not re-hit the surface it left", "[core][hit_record]") {
    Sampler sampler = make_sampler(301);

    for (const Float scale : scales) {
        const Quad surface = tilted_surface(scale);

        // Which side of its own plane a hit point lands on is fixed once the point
        // is computed, so one aim would only sample one draw of the rounding. The
        // face is swept instead.
        for (int i = 0; i < sweep; ++i) {
            for (int j = 0; j < sweep; ++j) {
                const Float a = (static_cast<Float>(i) + 0.5_f) / static_cast<Float>(sweep);
                const Float b = (static_cast<Float>(j) + 0.5_f) / static_cast<Float>(sweep);

                const HitRecord rec = hit_at(surface, scale, a, b);
                const Ray spawned = rec.spawn_ray(outward_direction(sampler, rec.geometric_normal), 0.0_f);

                HitRecord self;
                REQUIRE_FALSE(surface.hit(spawned, Interval(0.0_f, infinity), self));
            }
        }
    }
}

TEST_CASE("unoffset hit points do land behind their surface", "[core][hit_record]") {
    // Keeps the case above from going vacuous. Spawning from rec.p itself, at a
    // scale the renderer actually uses, leaves part of the sweep behind the plane
    // it just hit. The sweep and the seed are fixed, so this is a statement about
    // float, not a coin flip.
    Sampler sampler = make_sampler(302);
    constexpr Float scale = 10'000.0_f;
    const Quad surface = tilted_surface(scale);

    int self_hits = 0;
    for (int i = 0; i < sweep; ++i) {
        for (int j = 0; j < sweep; ++j) {
            const Float a = (static_cast<Float>(i) + 0.5_f) / static_cast<Float>(sweep);
            const Float b = (static_cast<Float>(j) + 0.5_f) / static_cast<Float>(sweep);

            const HitRecord rec = hit_at(surface, scale, a, b);
            const Ray unoffset(rec.p, outward_direction(sampler, rec.geometric_normal));

            HitRecord self;
            if (surface.hit(unoffset, Interval(0.0_f, infinity), self)) {
                self_hits++;
            }
        }
    }

    REQUIRE(self_hits > 0);
}
