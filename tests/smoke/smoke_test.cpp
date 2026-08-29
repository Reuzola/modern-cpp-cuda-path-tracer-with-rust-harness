#include "pt/core/hit_record.hpp"
#include "pt/core/hittable_list.hpp"
#include "pt/geometry/sphere.hpp"
#include "pt/io/color.hpp"
#include "pt/materials/lambertian.hpp"
#include "pt/math/aabb.hpp"
#include "pt/math/color.hpp"
#include "pt/math/constants.hpp"
#include "pt/math/interval.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/sampler.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include "pt/post/tonemap.hpp"
#include "pt/render/film.hpp"
#include "pt/render/tile.hpp"
#include "pt/sampling/sphere_pdf.hpp"
#include "pt/scene/scene_error.hpp"
#include "pt/textures/solid_color.hpp"
#include "pt/util/log.hpp"
#include "support/test_support.hpp"
#include <array>
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <limits>
#include <type_traits>

// The build's own test. Every other suite checks what the engine computes; this
// one checks that the engine the test binary is linked against was configured the
// way the test binary thinks it was. Those failures do not look like failures -
// they look like arbitrary numbers in an unrelated suite - so they are caught
// here, cheaply, with a message that names the cause.

namespace {

using pt::Float;
using pt::operator""_f;

} // namespace

TEST_CASE("the scalar type follows the build option", "[smoke][config]") {
#ifdef PT_FLOAT_AS_DOUBLE
    STATIC_REQUIRE(std::is_same_v<Float, double>);
#else
    STATIC_REQUIRE(std::is_same_v<Float, float>);
#endif

    // The literal suffix and the alias have to agree, or a `0.5_f` written in a
    // hot loop silently converts on every evaluation.
    STATIC_REQUIRE(std::is_same_v<decltype(0.5_f), Float>);

    // And the test suite's tolerance is derived from the same alias rather than
    // fixed, so switching precision does not quietly turn every comparison into
    // an exact one.
    STATIC_REQUIRE(pt_test::tolerance == (std::is_same_v<Float, double> ? 1e-6 : 1e-4));
}

TEST_CASE("the engine and the test binary agree on the scalar type", "[smoke][config]") {
    // The macro is PUBLIC on the core library, so consumers inherit it. If that
    // ever became PRIVATE, the engine would be compiled with one Float and every
    // consumer with another: Vec3, Ray and HitRecord would have different layouts
    // on the two sides of a call, the linker would say nothing, and the result
    // would be garbage that changes with the optimiser's mood.
    //
    // Values chosen to be exact in both float and double, and compared exactly:
    // a layout mismatch does not produce a small error.
    const pt::Sphere sphere{pt::Point3(2, -4, 8), 1.0_f, nullptr};
    const pt::Aabb box = sphere.bounding_box();

    REQUIRE(box.x.min == 1.0_f);
    REQUIRE(box.y.max == -3.0_f);
    REQUIRE(box.z.min == 7.0_f);

    // The same check across the io boundary, where a Color goes in and bytes come
    // back out.
    REQUIRE(pt::to_ldr_bytes(pt::Color(0.0_f, 0.25_f, 0.5_f)) == std::array<std::uint8_t, 3>{0, 64, 128});
}

TEST_CASE("floating point keeps its special values", "[smoke][config]") {
    constexpr Float nan = std::numeric_limits<Float>::quiet_NaN();

    // A fast-math flag anywhere in the chain makes these fold to constants and
    // takes several load-bearing behaviours with them: the slab test relies on a
    // NaN losing every comparison so a box stays conservatively accepted, and
    // tone mapping relies on a NaN being detectable at all. Neither would fail
    // visibly - the first drops geometry, the second writes a bright speck.
    REQUIRE(nan != nan);
    REQUIRE_FALSE(nan < 1.0_f);
    REQUIRE_FALSE(nan > 1.0_f);

    REQUIRE(pt::infinity > std::numeric_limits<Float>::max());
    REQUIRE(-pt::infinity < std::numeric_limits<Float>::lowest());

    // The engine's own view of the same thing: a NaN sample has to reach the film
    // as black. This crosses into the post-processing translation unit, which is
    // compiled with the library's flags rather than the test's.
    pt::Film film(1, 1);
    film.set_pixel(0, 0, pt::Color(nan, nan, nan));
    const pt::Film mapped = pt::tone_map(film, pt::ToneMapSettings{});
    REQUIRE(mapped.pixel(0, 0).r() == 0.0_f);
}

TEST_CASE("every engine layer is linked into the test binary", "[smoke][config]") {
    // One out-of-line symbol per layer. Each has its own suite, so this adds no
    // coverage - what it adds is a single, legible failure when a source file is
    // dropped from the library's file list or a layer stops being compiled at
    // all, instead of a scattered handful of unrelated suites going red.
    pt::Sampler sampler{1234};

    REQUIRE(pt::log_level() == pt::LogLevel::info);                                                                                                                              // util
    REQUIRE(pt::Vec3::random(sampler).length_squared() >= 0.0_f);                                                                                                                // math
    REQUIRE(pt::HittableList{}.empty());                                                                                                                                         // core
    REQUIRE(pt::Sphere(pt::Point3(0, 0, 0), 1.0_f, nullptr).bounding_box().x.size() > 0.0_f);                                                                                    // geometry
    REQUIRE(pt::SolidColor(pt::Color(1, 1, 1)).value(0, 0, pt::Point3(0, 0, 0)).r() == 1.0_f);                                                                                   // textures
    REQUIRE(pt::Lambertian(nullptr).scattering_pdf(pt::Ray(pt::Point3(0, 0, 0), pt::Vec3(0, 0, 1)), pt::HitRecord{}, pt::Ray(pt::Point3(0, 0, 0), pt::Vec3(0, 0, 1))) == 0.0_f); // materials
    REQUIRE(pt::SpherePdf{}.value(pt::Vec3(0, 0, 1)) > 0.0_f);                                                                                                                   // sampling
    REQUIRE(pt::make_tiles(4, 4, 2).size() == 4);                                                                                                                                // render
    REQUIRE(pt::SceneError("linked").location().empty());                                                                                                                        // scene

    // io and post are exercised above, where they carry a Float across the
    // library boundary.
}

TEST_CASE("the empty interval is empty and the universe is not", "[smoke][config]") {
    // The oldest two assertions in the suite, kept because they still say
    // something: the default Interval is inverted rather than zero-sized, and
    // that is what makes it the identity of a hull and a rejecting bound.
    STATIC_REQUIRE_FALSE(pt::Interval{}.contains(0.0_f));
    STATIC_REQUIRE(pt::Interval::universe.contains(0.0_f));
}
