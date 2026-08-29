#include "pt/math/onb.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include "support/test_support.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

namespace {

using pt::Onb;
using pt::Vec3;
using pt::operator""_f;
using pt_test::require_near;
using pt_test::require_vec_near;

} // namespace

TEST_CASE("the basis is orthonormal for any normal", "[math][onb]") {
    // Both branches of the fallback are covered: the first three normals are
    // dominated by x and take the (0, 1, 0) reference vector, the rest take
    // (1, 0, 0). Picking a reference parallel to the normal would produce a
    // zero cross product, which is what the branch avoids.
    const Vec3 normal = GENERATE(Vec3(1.0_f, 0.0_f, 0.0_f),
                                 Vec3(-1.0_f, 0.0_f, 0.0_f),
                                 Vec3(0.95_f, 0.1_f, 0.1_f),
                                 Vec3(0.0_f, 1.0_f, 0.0_f),
                                 Vec3(0.0_f, 0.0_f, 1.0_f),
                                 Vec3(0.0_f, 0.0_f, -1.0_f),
                                 Vec3(1.0_f, 1.0_f, 1.0_f),
                                 Vec3(-0.3_f, 0.7_f, -0.6_f));

    const Onb basis(normal);

    require_near(basis.u().length(), 1.0_f);
    require_near(basis.v().length(), 1.0_f);
    require_near(basis.w().length(), 1.0_f);

    require_near(dot(basis.u(), basis.v()), 0.0_f);
    require_near(dot(basis.v(), basis.w()), 0.0_f);
    require_near(dot(basis.w(), basis.u()), 0.0_f);
}

TEST_CASE("w is the normalised normal", "[math][onb]") {
    // Everything downstream assumes the third axis is the surface normal:
    // CosinePdf builds its hemisphere around it and measures its cosine
    // against it.
    const Vec3 normal(2.0_f, -4.0_f, 4.0_f);
    const Onb basis(normal);

    require_vec_near(basis.w(), unit_vector(normal));

    SECTION("an unnormalised input is accepted") {
        require_near(Onb(100.0_f * normal).w().length(), 1.0_f);
        require_vec_near(Onb(100.0_f * normal).w(), unit_vector(normal));
    }
}

TEST_CASE("the basis is left-handed, by construction", "[math][onb]") {
    // u is built as w x v, which makes u x v come out as -w rather than +w.
    // Recorded here as the convention rather than corrected: cosine sampling
    // draws its azimuth uniformly, so the handedness cannot change the
    // distribution, but flipping u would negate one component of every
    // generated direction and invalidate the whole golden set.
    const Onb basis(Vec3(0.0_f, 0.0_f, 1.0_f));

    require_vec_near(cross(basis.u(), basis.v()), -basis.w());
}

TEST_CASE("transform maps local coordinates onto the basis", "[math][onb]") {
    const Onb basis(Vec3(1.0_f, 2.0_f, 3.0_f));

    SECTION("the local axes come back as the basis vectors") {
        require_vec_near(basis.transform(Vec3(1.0_f, 0.0_f, 0.0_f)), basis.u());
        require_vec_near(basis.transform(Vec3(0.0_f, 1.0_f, 0.0_f)), basis.v());
        require_vec_near(basis.transform(Vec3(0.0_f, 0.0_f, 1.0_f)), basis.w());
    }
    SECTION("the transform is a rotation, so it preserves length") {
        const Vec3 local(0.3_f, -0.5_f, 0.8_f);
        require_near(basis.transform(local).length(), local.length());
    }
    SECTION("it preserves angles too") {
        const Vec3 a(0.3_f, -0.5_f, 0.8_f);
        const Vec3 b(-0.2_f, 0.9_f, 0.1_f);
        require_near(dot(basis.transform(a), basis.transform(b)), dot(a, b));
    }
    SECTION("the local z component is the cosine against the normal") {
        // This is the identity CosinePdf::value relies on: a direction
        // generated with local z = cos(theta) has exactly that cosine against
        // the surface normal once transformed.
        const Vec3 local = unit_vector(Vec3(0.4_f, 0.3_f, 0.6_f));
        require_near(dot(basis.transform(local), basis.w()), local.z());
    }
}
