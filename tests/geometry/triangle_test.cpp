#include "pt/core/hit_record.hpp"
#include "pt/geometry/triangle.hpp"
#include "pt/materials/lambertian.hpp"
#include "pt/math/aabb.hpp"
#include "pt/math/color.hpp"
#include "pt/math/constants.hpp"
#include "pt/math/interval.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include "pt/textures/solid_color.hpp"
#include "support/test_support.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

namespace {

using pt::Float;
using pt::HitRecord;
using pt::Interval;
using pt::Point3;
using pt::Ray;
using pt::Triangle;
using pt::TriangleHit;
using pt::Vec3;
using pt::operator""_f;
using pt_test::require_vec_near;
using pt_test::tolerance;
using pt_test::widen;

using Catch::Matchers::WithinAbs;


// Reference triangle: the unit right triangle in the z = 0 plane, wound so that
// cross(v1 - v0, v2 - v0) points towards +z.
const Point3 v0{0.0_f, 0.0_f, 0.0_f};
const Point3 v1{1.0_f, 0.0_f, 0.0_f};
const Point3 v2{0.0_f, 1.0_f, 0.0_f};

// Material is never dereferenced by the intersection path, so most cases pass
// nullptr rather than building a texture they do not use.
[[nodiscard]] Triangle reference_triangle() { return Triangle(v0, v1, v2, nullptr); }

// A ray shot straight down at (x, y, 0) from one unit above the plane.
[[nodiscard]] Ray ray_from_above(Float x, Float y) {
    return Ray(Point3(x, y, 1.0_f), Vec3(0.0_f, 0.0_f, -1.0_f));
}

const Interval visible{0.001_f, pt::infinity};

} // namespace

TEST_CASE("an interior hit reports distance, barycentrics and position consistently", "[geometry][triangle]") {
    const Triangle tri = reference_triangle();
    HitRecord rec;

    REQUIRE(tri.hit(ray_from_above(0.25_f, 0.25_f), visible, rec));

    REQUIRE_THAT(widen(rec.t), WithinAbs(1.0, tolerance));
    REQUIRE_THAT(widen(rec.u), WithinAbs(0.25, tolerance));
    REQUIRE_THAT(widen(rec.v), WithinAbs(0.25, tolerance));
    require_vec_near(rec.p, Point3(0.25_f, 0.25_f, 0.0_f));

    // The same point, rebuilt from the barycentric weights instead of from t.
    const Point3 reconstructed = v0 + rec.u * (v1 - v0) + rec.v * (v2 - v0);
    require_vec_near(rec.p, reconstructed);
}

TEST_CASE("barycentric weights identify the triangle's vertices", "[geometry][triangle]") {
    const Triangle tri = reference_triangle();
    HitRecord rec;

    Float expected_u = 0.0_f;
    Float expected_v = 0.0_f;
    Point3 target = v0;

    SECTION("v0 carries weight (0, 0)") {
        target = v0;
        expected_u = 0.0_f;
        expected_v = 0.0_f;
    }
    SECTION("v1 carries weight (1, 0)") {
        target = v1;
        expected_u = 1.0_f;
        expected_v = 0.0_f;
    }
    SECTION("v2 carries weight (0, 1)") {
        target = v2;
        expected_u = 0.0_f;
        expected_v = 1.0_f;
    }
    SECTION("a point on the hypotenuse carries weights summing to one") {
        target = Point3(0.5_f, 0.5_f, 0.0_f);
        expected_u = 0.5_f;
        expected_v = 0.5_f;
    }

    REQUIRE(tri.hit(ray_from_above(target.x(), target.y()), visible, rec));
    REQUIRE_THAT(widen(rec.u), WithinAbs(widen(expected_u), tolerance));
    REQUIRE_THAT(widen(rec.v), WithinAbs(widen(expected_v), tolerance));
}

TEST_CASE("a ray outside the triangle's edges misses", "[geometry][triangle]") {
    const Triangle tri = reference_triangle();
    HitRecord rec;

    Float x = 0.0_f;
    Float y = 0.0_f;

    SECTION("beyond the hypotenuse, where the weights sum above one") {
        x = 0.6_f;
        y = 0.6_f;
    }
    SECTION("past the v0-v2 edge, where the first weight is negative") {
        x = -0.1_f;
        y = 0.5_f;
    }
    SECTION("past the v0-v1 edge, where the second weight is negative") {
        x = 0.5_f;
        y = -0.1_f;
    }

    REQUIRE_FALSE(tri.hit(ray_from_above(x, y), visible, rec));
}

TEST_CASE("a ray parallel to the triangle's plane misses", "[geometry][triangle]") {
    const Triangle tri = reference_triangle();
    HitRecord rec;

    SECTION("offset from the plane") {
        const Ray r(Point3(0.25_f, 0.25_f, 1.0_f), Vec3(1.0_f, 0.0_f, 0.0_f));
        REQUIRE_FALSE(tri.hit(r, visible, rec));
    }
    SECTION("lying in the plane") {
        const Ray r(Point3(-1.0_f, 0.25_f, 0.0_f), Vec3(1.0_f, 0.0_f, 0.0_f));
        REQUIRE_FALSE(tri.hit(r, visible, rec));
    }
}

TEST_CASE("a degenerate triangle is never hit", "[geometry][triangle]") {
    HitRecord rec;

    SECTION("collinear vertices") {
        const Triangle tri(Point3(0.0_f, 0.0_f, 0.0_f), Point3(1.0_f, 0.0_f, 0.0_f), Point3(2.0_f, 0.0_f, 0.0_f), nullptr);
        REQUIRE_FALSE(tri.hit(ray_from_above(0.5_f, 0.0_f), visible, rec));
    }
    SECTION("coincident vertices") {
        const Triangle tri(v0, v0, v0, nullptr);
        REQUIRE_FALSE(tri.hit(ray_from_above(0.0_f, 0.0_f), visible, rec));
    }
}

TEST_CASE("hits outside the ray interval are rejected", "[geometry][triangle]") {
    const Triangle tri = reference_triangle();
    HitRecord rec;
    const Ray r = ray_from_above(0.25_f, 0.25_f); // hits at t = 1

    SECTION("the interval contains the hit") {
        REQUIRE(tri.hit(r, Interval(0.5_f, 2.0_f), rec));
    }
    SECTION("the interval ends before the hit") {
        REQUIRE_FALSE(tri.hit(r, Interval(0.001_f, 0.5_f), rec));
    }
    SECTION("the interval starts after the hit") {
        REQUIRE_FALSE(tri.hit(r, Interval(2.0_f, pt::infinity), rec));
    }
    SECTION("the triangle is behind the ray's origin") {
        const Ray away(Point3(0.25_f, 0.25_f, 1.0_f), Vec3(0.0_f, 0.0_f, 1.0_f));
        REQUIRE_FALSE(tri.hit(away, visible, rec));
    }
}

TEST_CASE("a triangle is hit from both sides and flips only the shading normal", "[geometry][triangle]") {
    const Triangle tri = reference_triangle();
    const Vec3 geometric_normal{0.0_f, 0.0_f, 1.0_f};
    HitRecord rec;

    SECTION("front face, hit from +z") {
        const Ray r = ray_from_above(0.25_f, 0.25_f);
        REQUIRE(tri.hit(r, visible, rec));
        REQUIRE(rec.front_face);
        require_vec_near(rec.normal, geometric_normal);

        TriangleHit out;
        REQUIRE(pt::intersect_triangle(r, visible, v0, v1, v2, out));
        require_vec_near(out.normal, geometric_normal);
    }
    SECTION("back face, hit from -z") {
        const Ray r(Point3(0.25_f, 0.25_f, -1.0_f), Vec3(0.0_f, 0.0_f, 1.0_f));
        REQUIRE(tri.hit(r, visible, rec));
        REQUIRE_FALSE(rec.front_face);
        require_vec_near(rec.normal, -geometric_normal);

        // The geometric normal follows the winding order, not the viewing side.
        TriangleHit out;
        REQUIRE(pt::intersect_triangle(r, visible, v0, v1, v2, out));
        require_vec_near(out.normal, geometric_normal);
    }
}

TEST_CASE("the bounding box encloses every vertex", "[geometry][triangle]") {
    SECTION("a triangle in general position") {
        const Triangle tri(Point3(1.0_f, 2.0_f, 3.0_f), Point3(-4.0_f, 5.0_f, 0.0_f), Point3(2.0_f, -1.0_f, 7.0_f), nullptr);
        const pt::Aabb bbox = tri.bounding_box();

        REQUIRE_THAT(widen(bbox.x.min), WithinAbs(-4.0, tolerance));
        REQUIRE_THAT(widen(bbox.x.max), WithinAbs(2.0, tolerance));
        REQUIRE_THAT(widen(bbox.y.min), WithinAbs(-1.0, tolerance));
        REQUIRE_THAT(widen(bbox.y.max), WithinAbs(5.0, tolerance));
        REQUIRE_THAT(widen(bbox.z.min), WithinAbs(0.0, tolerance));
        REQUIRE_THAT(widen(bbox.z.max), WithinAbs(7.0, tolerance));
    }
    SECTION("a planar triangle still has a non-empty box on the flat axis") {
        const pt::Aabb bbox = reference_triangle().bounding_box();
        REQUIRE(bbox.z.size() > 0.0_f);
    }
}

TEST_CASE("the hit record carries the triangle's material", "[geometry][triangle]") {
    const pt::SolidColor albedo{pt::Color(0.5_f, 0.5_f, 0.5_f)};
    const pt::Lambertian mat{&albedo};
    const Triangle tri(v0, v1, v2, &mat);
    HitRecord rec;

    REQUIRE(tri.hit(ray_from_above(0.25_f, 0.25_f), visible, rec));
    REQUIRE(rec.mat == &mat);
}
