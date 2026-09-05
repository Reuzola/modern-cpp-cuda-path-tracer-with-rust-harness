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

// A quad tilted off every axis and pushed out to `scale`, so the hit point and
// the plane test both carry rounding error in all three components. An axis
// aligned quad at a round coordinate is arithmetically exact and proves nothing.
[[nodiscard]] Quad tilted_surface(Float scale) {
    return Quad(Point3(scale - 1, scale - 1, scale), Vec3(2, 0.3_f, 0.7_f), Vec3(0.3_f, 2, -0.4_f), nullptr);
}

// Its centre, where the incoming ray is aimed.
[[nodiscard]] Point3 surface_centre(Float scale) {
    return Point3(scale + 0.15_f, scale + 0.15_f, scale + 0.15_f);
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

TEST_CASE("the geometric normal is stored facing the ray, like the shading one", "[core][hit_record]") {
    HitRecord rec;

    const Vec3 geometric(0, 0, 1);        // away from the ray: a back face
    const Vec3 shading(0, 0.6_f, -0.8_f); // towards the ray

    rec.set_face_normal(forward, geometric, shading);

    // One convention for both normals is what lets spawn_ray pick the offset side
    // from the scatter direction alone, with no front_face branch at the call site.
    REQUIRE_FALSE(rec.front_face);
    require_vec_near(rec.geometric_normal, Vec3(0, 0, -1));
    REQUIRE(dot(rec.normal, rec.geometric_normal) > 0.0_f);

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

        // Hit it the way a camera does: unit direction, long t, so the hit point
        // carries the full rounding error of origin + t * direction.
        const Point3 eye(0, 0, 0);
        const Ray incoming(eye, unit_vector(surface_centre(scale) - eye));

        HitRecord rec;
        REQUIRE(surface.hit(incoming, Interval(0.0_f, infinity), rec));

        // Every direction here leaves on the outward side, and a plane cannot be
        // hit from the side you are walking away from. So any hit at all means the
        // origin ended up behind its own surface, which is what acne is.
        for (int i = 0; i < 64; ++i) {
            Vec3 direction = random_unit_vector(sampler);
            if (dot(direction, rec.geometric_normal) < 0.0_f) {
                direction = -direction;
            }

            HitRecord self;
            REQUIRE_FALSE(surface.hit(rec.spawn_ray(direction, 0.0_f), Interval(0.0_f, infinity), self));
        }
    }
}

TEST_CASE("the unoffset hit point really does land behind its surface", "[core][hit_record]") {
    // Keeps the case above from going vacuous. Spawning from rec.p itself, at a
    // scale the renderer actually uses, puts the origin on the wrong side for a
    // large share of the directions leaving it. The seed is fixed, so this is a
    // statement about float, not a coin flip.
    Sampler sampler = make_sampler(302);

    constexpr Float scale = 10'000.0_f;
    const Quad surface = tilted_surface(scale);
    const Point3 eye(0, 0, 0);

    HitRecord rec;
    REQUIRE(surface.hit(Ray(eye, unit_vector(surface_centre(scale) - eye)), Interval(0.0_f, infinity), rec));

    int self_hits = 0;
    for (int i = 0; i < 64; ++i) {
        Vec3 direction = random_unit_vector(sampler);
        if (dot(direction, rec.geometric_normal) < 0.0_f) {
            direction = -direction;
        }

        HitRecord self;
        if (surface.hit(Ray(rec.p, direction), Interval(0.0_f, infinity), self)) {
            self_hits++;
        }
    }

    REQUIRE(self_hits > 0);
}
