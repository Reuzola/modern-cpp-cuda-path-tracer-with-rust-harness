#include "pt/core/hit_record.hpp"
#include "pt/geometry/instance.hpp"
#include "pt/geometry/sphere.hpp"
#include "pt/math/constants.hpp"
#include "pt/math/interval.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/transform.hpp"
#include "pt/math/vec3.hpp"
#include "support/test_support.hpp"
#include <catch2/catch_test_macros.hpp>

namespace {

using pt::HitRecord;
using pt::Instance;
using pt::Interval;
using pt::Point3;
using pt::Ray;
using pt::Sphere;
using pt::Transform;
using pt::Vec3;
using pt::operator""_f;
using pt_test::require_aabb_near;
using pt_test::require_near;
using pt_test::require_vec_near;

const Interval visible{0.001_f, pt::infinity};

// A unit sphere at the origin: every expectation below is about what the
// transform did to it, never about the sphere itself.
const Sphere unit_sphere{Point3(0, 0, 0), 1.0_f, nullptr};

} // namespace

TEST_CASE("a translated instance is hit where the transform put it", "[geometry][instance]") {
    const Instance moved{&unit_sphere, Transform::translation(Vec3(0, 0, 5))};

    HitRecord rec;
    REQUIRE(moved.hit(Ray(Point3(0, 0, 0), Vec3(0, 0, 1)), visible, rec));

    // The record comes back in world space: the integrator spawns the next ray
    // from rec.p and knows nothing about the child's object space.
    require_near(rec.t, 4.0_f);
    require_vec_near(rec.p, Point3(0, 0, 4));
    require_vec_near(rec.normal, Vec3(0, 0, -1));
    REQUIRE(rec.front_face);

    require_aabb_near(moved.bounding_box(), Point3(-1, -1, 4), Point3(1, 1, 6));
}

TEST_CASE("t means the same distance in both spaces", "[geometry][instance]") {
    const Instance grown{&unit_sphere, Transform::scaling(Vec3(2, 2, 2))};

    // A sphere of radius 2 at the origin, so a unit-direction ray from 10 units
    // away stops at 8.
    HitRecord rec;
    REQUIRE(grown.hit(Ray(Point3(0, 0, -10), Vec3(0, 0, 1)), visible, rec));
    require_near(rec.t, 8.0_f);

    // The inverse-transformed direction is left unnormalised on purpose: it comes
    // out half as long, the child reports twice the parameter, and the two
    // cancel. Normalising it would make rec.t an object-space distance, so every
    // scaled instance would break both the caller's interval and the sort order
    // inside the BVH - while still rendering correctly on its own.
    REQUIRE_FALSE(grown.hit(Ray(Point3(0, 0, -10), Vec3(0, 0, 1)), Interval(0.001_f, 7.9_f), rec));
    REQUIRE(grown.hit(Ray(Point3(0, 0, -10), Vec3(0, 0, 1)), Interval(0.001_f, 8.1_f), rec));
}

TEST_CASE("normals use the inverse transpose under non-uniform scale", "[geometry][instance]") {
    // Twice as wide in x: the unit sphere becomes an ellipsoid.
    const Instance stretched{&unit_sphere, Transform::scaling(Vec3(2, 1, 1))};

    // Aimed so that it lands on the object-space point (0.7071, 0.7071, 0),
    // which the transform carries to (1.4142, 0.7071, 0) - a place where the
    // surface is genuinely slanted.
    const Ray oblique{Point3(7.0710678_f, 3.5355339_f, 0), Vec3(-1.4142136_f, -0.7071068_f, 0)};

    HitRecord rec;
    REQUIRE(stretched.hit(oblique, visible, rec));

    require_near(rec.t, 4.0_f, 1e-3);
    require_vec_near(rec.p, Point3(1.4142136_f, 0.7071068_f, 0), 1e-3);

    // The correct normal is (0.4472, 0.8944, 0). Pushing the object-space normal
    // through the linear part instead would give (0.8944, 0.4472, 0) - the two
    // swap places here, so this is the configuration that tells them apart. Every
    // uniformly scaled or purely rotated instance agrees under both rules, which
    // is why an inverse-transpose bug survives most scenes.
    require_vec_near(rec.normal, Vec3(0.4472136_f, 0.8944272_f, 0), 1e-3);

    // Renormalised after the transform: a scale would otherwise leave a normal
    // whose length silently scales every cosine term downstream.
    require_near(rec.normal.length(), 1.0_f, 1e-3);
}

TEST_CASE("a rotated instance carries its child around", "[geometry][instance]") {
    const Sphere off_axis{Point3(0, 0, 5), 1.0_f, nullptr};
    const Instance turned{&off_axis, Transform::rotation_y(90.0_f)};

    // A quarter turn about +y takes +z to +x.
    HitRecord rec;
    REQUIRE(turned.hit(Ray(Point3(0, 0, 0), Vec3(1, 0, 0)), visible, rec));
    require_near(rec.t, 4.0_f, 1e-3);
    require_vec_near(rec.p, Point3(4, 0, 0), 1e-3);

    require_aabb_near(turned.bounding_box(), Point3(4, -1, -1), Point3(6, 1, 1), 1e-3);
}

TEST_CASE("instances nest, outermost transform last", "[geometry][instance]") {
    const Instance inner{&unit_sphere, Transform::translation(Vec3(3, 0, 0))};
    const Instance outer{&inner, Transform::rotation_y(90.0_f)};

    // The child is moved to +x, then the rotation carries +x to -z.
    HitRecord rec;
    REQUIRE(outer.hit(Ray(Point3(0, 0, -8), Vec3(0, 0, 1)), visible, rec));
    require_near(rec.t, 4.0_f, 1e-3);
    require_vec_near(rec.p, Point3(0, 0, -4), 1e-3);

    // Sharing the child rather than owning it is what makes this cheap: the same
    // sphere is behind both instances, and a mesh behind fifty of them.
    require_aabb_near(outer.bounding_box(), Point3(-1, -1, -4), Point3(1, 1, -2), 1e-3);
}

TEST_CASE("a mirroring transform does not invert orientation", "[geometry][instance]") {
    const Sphere to_the_right{Point3(3, 0, 5), 1.0_f, nullptr};
    const Instance mirrored{&to_the_right, Transform::scaling(Vec3(-1, 1, 1))};

    HitRecord rec;
    REQUIRE(mirrored.hit(Ray(Point3(-8, 0, 5), Vec3(1, 0, 0)), visible, rec));

    require_near(rec.t, 4.0_f);
    require_vec_near(rec.p, Point3(-4, 0, 5));

    // front_face is taken from the child's object-space test and never
    // recomputed. A mirror flips the normal and flips the direction with it, so
    // the two stay on the same side - the outside of a mirrored sphere is still
    // its outside, and a dielectric behind one still picks the right index ratio.
    require_vec_near(rec.normal, Vec3(-1, 0, 0));
    REQUIRE(rec.front_face);
}

TEST_CASE("a miss leaves the record untouched", "[geometry][instance]") {
    const Instance moved{&unit_sphere, Transform::translation(Vec3(0, 0, 5))};

    HitRecord rec;
    rec.t = 42.0_f;

    REQUIRE_FALSE(moved.hit(Ray(Point3(0, 3, 0), Vec3(0, 0, 1)), visible, rec));
    require_near(rec.t, 42.0_f);
}
