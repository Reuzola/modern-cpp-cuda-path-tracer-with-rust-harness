#include "pt/core/hit_record.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include "support/test_support.hpp"
#include <catch2/catch_test_macros.hpp>

namespace {

using pt::HitRecord;
using pt::Point3;
using pt::Ray;
using pt::Vec3;
using pt::operator""_f;
using pt_test::require_vec_near;

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
    const Vec3 geometric(0, 0, 1);  // away from the ray: a back face
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
