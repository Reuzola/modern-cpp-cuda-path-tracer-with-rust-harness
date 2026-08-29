#include "pt/math/aabb.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/transform.hpp"
#include "pt/math/vec3.hpp"
#include "support/test_support.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

namespace {

using pt::Aabb;
using pt::Mat3;
using pt::Point3;
using pt::Transform;
using pt::Vec3;
using pt::operator""_f;
using pt_test::require_aabb_near;
using pt_test::require_near;
using pt_test::require_vec_near;

} // namespace

TEST_CASE("Mat3 defaults to the identity", "[math][transform]") {
    const Vec3 v(1.0_f, 2.0_f, 3.0_f);
    require_vec_near(Mat3() * v, v);
}

TEST_CASE("transpose swaps rows and columns", "[math][transform]") {
    const Mat3 m(Vec3(1.0_f, 2.0_f, 3.0_f), Vec3(4.0_f, 5.0_f, 6.0_f), Vec3(7.0_f, 8.0_f, 9.0_f));
    const Mat3 t = transpose(m);

    require_vec_near(t.row(0), Vec3(1.0_f, 4.0_f, 7.0_f));
    require_vec_near(t.row(2), Vec3(3.0_f, 6.0_f, 9.0_f));

    SECTION("transposing twice is the identity") {
        require_vec_near(transpose(t).row(1), m.row(1));
    }
    SECTION("a rotation's transpose is its inverse") {
        // Every rotation factory relies on this: the inverse is built by
        // transposing rather than by inverting.
        const Vec3 v(0.3_f, -0.7_f, 0.5_f);
        const Mat3 r(Vec3(0.0_f, -1.0_f, 0.0_f), Vec3(1.0_f, 0.0_f, 0.0_f), Vec3(0.0_f, 0.0_f, 1.0_f));

        require_vec_near(transpose(r) * (r * v), v);
    }
}

TEST_CASE("every transform round-trips through its own inverse", "[math][transform]") {
    // The inverse is constructed analytically, never derived from the forward
    // matrix, so nothing but a test makes the two agree. Instance intersection
    // pulls the ray through the inverse and pushes the hit back through the
    // forward transform, so a mismatch would place surfaces somewhere else
    // entirely.
    const Transform t = GENERATE(Transform(),
                                 Transform::translation(Vec3(3.0_f, -1.0_f, 2.0_f)),
                                 Transform::scaling(Vec3(2.0_f, 0.5_f, -3.0_f)),
                                 Transform::rotation_x(37.0_f),
                                 Transform::rotation_y(-115.0_f),
                                 Transform::rotation_z(90.0_f),
                                 Transform::translation(Vec3(1.0_f, 2.0_f, 3.0_f)) * Transform::rotation_y(45.0_f) * Transform::scaling(Vec3(2.0_f, 2.0_f, 2.0_f)));

    const Point3 p(0.7_f, -2.3_f, 1.1_f);
    const Vec3 v(0.4_f, 0.9_f, -0.2_f);

    require_vec_near(t.apply_inverse_point(t.apply_point(p)), p);
    require_vec_near(t.apply_point(t.apply_inverse_point(p)), p);
    require_vec_near(t.apply_inverse_vector(t.apply_vector(v)), v);
}

TEST_CASE("points translate but vectors do not", "[math][transform]") {
    // A direction is defined by a difference of two points, so a shift of the
    // origin cancels out. Ray directions travel this path; sending them through
    // apply_point instead would bend every transformed ray.
    const Transform t = Transform::translation(Vec3(5.0_f, 0.0_f, 0.0_f));
    const Vec3 v(1.0_f, 2.0_f, 3.0_f);

    require_vec_near(t.apply_point(Point3(0.0_f, 0.0_f, 0.0_f)), Point3(5.0_f, 0.0_f, 0.0_f));
    require_vec_near(t.apply_vector(v), v);
    require_vec_near(t.apply_inverse_vector(v), v);
}

TEST_CASE("the rotations turn about their own axis in a fixed direction", "[math][transform]") {
    // The scene format names an angle in degrees per axis; which way that angle
    // turns is a convention the format documents and the loader depends on.
    SECTION("rotation_y sends +x towards -z") {
        require_vec_near(Transform::rotation_y(90.0_f).apply_point(Point3(1.0_f, 0.0_f, 0.0_f)), Point3(0.0_f, 0.0_f, -1.0_f));
    }
    SECTION("rotation_x sends +y towards +z") {
        require_vec_near(Transform::rotation_x(90.0_f).apply_point(Point3(0.0_f, 1.0_f, 0.0_f)), Point3(0.0_f, 0.0_f, 1.0_f));
    }
    SECTION("rotation_z sends +x towards +y") {
        require_vec_near(Transform::rotation_z(90.0_f).apply_point(Point3(1.0_f, 0.0_f, 0.0_f)), Point3(0.0_f, 1.0_f, 0.0_f));
    }
    SECTION("a rotation leaves its own axis alone") {
        require_vec_near(Transform::rotation_y(37.0_f).apply_point(Point3(0.0_f, 1.0_f, 0.0_f)), Point3(0.0_f, 1.0_f, 0.0_f));
    }
    SECTION("a full turn is the identity") {
        const Point3 p(1.0_f, 2.0_f, 3.0_f);
        require_vec_near(Transform::rotation_z(360.0_f).apply_point(p), p);
    }
    SECTION("rotation preserves distances") {
        const Point3 p(1.0_f, 2.0_f, 3.0_f);
        require_near(Transform::rotation_x(63.0_f).apply_point(p).length(), p.length());
    }
}

TEST_CASE("composition applies the right-hand operand first", "[math][transform]") {
    const Transform translate = Transform::translation(Vec3(10.0_f, 0.0_f, 0.0_f));
    const Transform scale = Transform::scaling(Vec3(2.0_f, 2.0_f, 2.0_f));

    SECTION("scale then translate") {
        // (1, 0, 0) scales to (2, 0, 0), then shifts to (12, 0, 0).
        require_vec_near((translate * scale).apply_point(Point3(1.0_f, 0.0_f, 0.0_f)), Point3(12.0_f, 0.0_f, 0.0_f));
    }
    SECTION("translate then scale") {
        // (1, 0, 0) shifts to (11, 0, 0), then scales to (22, 0, 0).
        require_vec_near((scale * translate).apply_point(Point3(1.0_f, 0.0_f, 0.0_f)), Point3(22.0_f, 0.0_f, 0.0_f));
    }
    SECTION("the composed inverse reverses the order") {
        // (AB) inverse is (B inverse)(A inverse), and getting this backwards
        // would still round-trip for commuting pairs - hence a pair that does
        // not commute.
        const Transform composed = translate * scale;
        const Point3 p(3.0_f, -1.0_f, 4.0_f);

        require_vec_near(composed.apply_inverse_point(composed.apply_point(p)), p);
    }
    SECTION("the scene format's order is scale, then rotate, then translate") {
        const Transform trs = Transform::translation(Vec3(0.0_f, 5.0_f, 0.0_f)) * Transform::rotation_y(90.0_f) * Transform::scaling(Vec3(3.0_f, 3.0_f, 3.0_f));

        // (1, 0, 0) -> (3, 0, 0) -> (0, 0, -3) -> (0, 5, -3).
        require_vec_near(trs.apply_point(Point3(1.0_f, 0.0_f, 0.0_f)), Point3(0.0_f, 5.0_f, -3.0_f));
    }
}

TEST_CASE("normals transform by the inverse transpose", "[math][transform]") {
    // Under non-uniform scale the linear part alone tilts a normal off the
    // surface. The defining property is not the matrix but the geometry: a
    // transformed normal must stay perpendicular to a transformed tangent.
    const Transform squash = Transform::scaling(Vec3(4.0_f, 1.0_f, 1.0_f));

    SECTION("a normal stays perpendicular to the surface it came from") {
        const Vec3 normal = unit_vector(Vec3(1.0_f, 1.0_f, 0.0_f));
        const Vec3 tangent(-1.0_f, 1.0_f, 0.0_f); // perpendicular to normal

        require_near(dot(squash.apply_normal(normal), squash.apply_vector(tangent)), 0.0_f);
    }
    SECTION("the naive transform would not") {
        // Confirms the case above is actually discriminating: pushing the
        // normal through apply_vector breaks the perpendicularity it checks.
        const Vec3 normal = unit_vector(Vec3(1.0_f, 1.0_f, 0.0_f));
        const Vec3 tangent(-1.0_f, 1.0_f, 0.0_f);

        REQUIRE(dot(squash.apply_vector(normal), squash.apply_vector(tangent)) != 0.0_f);
    }
    SECTION("the result is renormalised") {
        require_near(squash.apply_normal(unit_vector(Vec3(1.0_f, 2.0_f, 3.0_f))).length(), 1.0_f);
    }
    SECTION("under a rotation it matches the plain vector transform") {
        // The inverse transpose of a rotation is the rotation itself.
        const Transform rotate = Transform::rotation_z(50.0_f);
        const Vec3 normal = unit_vector(Vec3(0.0_f, 0.0_f, 1.0_f));

        require_vec_near(rotate.apply_normal(normal), rotate.apply_vector(normal));
    }
}

TEST_CASE("apply_bounds encloses the transformed box", "[math][transform]") {
    const Aabb unit_cube(Point3(-1.0_f, -1.0_f, -1.0_f), Point3(1.0_f, 1.0_f, 1.0_f));

    SECTION("a translation moves the box without growing it") {
        const Aabb moved = Transform::translation(Vec3(2.0_f, 3.0_f, 4.0_f)).apply_bounds(unit_cube);
        require_aabb_near(moved, Point3(1.0_f, 2.0_f, 3.0_f), Point3(3.0_f, 4.0_f, 5.0_f));
    }
    SECTION("an axis-aligned rotation is exact") {
        const Aabb turned = Transform::rotation_y(90.0_f).apply_bounds(unit_cube);
        require_aabb_near(turned, Point3(-1.0_f, -1.0_f, -1.0_f), Point3(1.0_f, 1.0_f, 1.0_f));
    }
    SECTION("an off-axis rotation grows the box, conservatively") {
        // A 45 degree turn about y widens the x and z extents to sqrt(2) while
        // leaving y alone. The result must contain the rotated geometry, which
        // is what makes an axis-aligned box of a rotated object conservative.
        const Aabb turned = Transform::rotation_y(45.0_f).apply_bounds(unit_cube);
        const pt::Float diagonal = 1.4142135_f;

        require_near(turned.x.max, diagonal, 1.0e-4);
        require_near(turned.z.max, diagonal, 1.0e-4);
        require_near(turned.y.max, 1.0_f);
    }
    SECTION("a flat box keeps a usable thickness through the transform") {
        // Computed in a single pass over all eight corners. Folding the corners
        // together with intermediate Aabbs would leak each one's degenerate-axis
        // padding into the result and make it depend on the order of the fold.
        const Aabb plane(Point3(-1.0_f, 0.0_f, -1.0_f), Point3(1.0_f, 0.0_f, 1.0_f));
        const Aabb moved = Transform::translation(Vec3(0.0_f, 5.0_f, 0.0_f)).apply_bounds(plane);

        REQUIRE(moved.y.size() > 0.0_f);
        require_near((moved.y.min + moved.y.max) / 2.0_f, 5.0_f, 1.0e-4);
        require_near(moved.x.size(), 2.0_f);
    }
}
