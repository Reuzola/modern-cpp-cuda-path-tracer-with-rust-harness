#include "pt/core/hit_record.hpp"
#include "pt/geometry/constant_medium.hpp"
#include "pt/geometry/quad.hpp"
#include "pt/geometry/sphere.hpp"
#include "pt/math/constants.hpp"
#include "pt/math/interval.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/sampler.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include "support/test_support.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cmath>

namespace {

using pt::ConstantMedium;
using pt::Float;
using pt::HitRecord;
using pt::Interval;
using pt::Point3;
using pt::Quad;
using pt::Ray;
using pt::Sampler;
using pt::Sphere;
using pt::Vec3;
using pt::operator""_f;
using pt_test::make_sampler;
using pt_test::require_near;
using pt_test::require_vec_near;
using pt_test::widen;

const Interval visible{0.001_f, pt::infinity};

// Large on purpose: the segment inside is 20 units long, so a sampled free path
// almost never runs past the far wall and the cases below stay deterministic
// rather than depending on which draw the seed happens to produce.
const Sphere cloud{Point3(0, 0, 15), 10.0_f, nullptr};

const Ray forward{Point3(0, 0, 0), Vec3(0, 0, 1)};

} // namespace

TEST_CASE("the interaction distance is the exponential free path", "[geometry][medium]") {
    constexpr Float density = 0.5_f;
    const ConstantMedium medium{&cloud, density, nullptr};

    // The same stream by hand: this pins the transform from a uniform draw to a
    // distance, and pins the number of draws to one at the same time.
    Sampler by_hand = make_sampler(31);
    const Float u = by_hand.next_scalar_positive();
    const Float expected_distance = -std::log(u) / density;

    Sampler sampler = make_sampler(31);
    HitRecord rec;
    REQUIRE(medium.sample_interaction(forward, visible, sampler, rec));

    // Entry is at 5, and the direction is a unit vector, so distance and t agree.
    require_near(rec.t, 5.0_f + expected_distance, 1e-3);
    REQUIRE(sampler.next_uint32() == by_hand.next_uint32());
}

TEST_CASE("the free path is a distance, not a ray parameter", "[geometry][medium]") {
    constexpr Float density = 0.5_f;
    const ConstantMedium medium{&cloud, density, nullptr};

    // Twice as long a direction: entry lands at t = 2.5 rather than 5, and the
    // sampled distance has to be divided by the direction's length before it can
    // be added to it. Getting that division wrong is invisible from the camera,
    // where directions are unit vectors, and wrong for every medium inside a
    // scaled instance.
    Sampler by_hand = make_sampler(32);
    const Float u = by_hand.next_scalar_positive();
    const Float expected_distance = -std::log(u) / density;

    Sampler sampler = make_sampler(32);
    HitRecord rec;
    REQUIRE(medium.sample_interaction(Ray(Point3(0, 0, 0), Vec3(0, 0, 2)), visible, sampler, rec));

    require_near(rec.t, 2.5_f + expected_distance / 2.0_f, 1e-3);
    require_vec_near(rec.p, Point3(0, 0, 5.0_f + expected_distance), 1e-2);
}

TEST_CASE("the record describes a scattering event, not a surface", "[geometry][medium]") {
    const ConstantMedium medium{&cloud, 0.5_f, nullptr};

    Sampler sampler = make_sampler(33);
    HitRecord rec;
    REQUIRE(medium.sample_interaction(forward, visible, sampler, rec));

    // The normal is arbitrary and front_face is unconditionally true: an
    // isotropic phase function scatters over the whole sphere and never consults
    // either. They are filled in only because the integrator reads a HitRecord.
    require_vec_near(rec.normal, Vec3(1, 0, 0));
    REQUIRE(rec.front_face);
    REQUIRE(rec.mat == nullptr);
}

TEST_CASE("a thin medium lets rays through", "[geometry][medium]") {
    // Mean free path of a thousand units against a segment of twenty: the sampled
    // distance runs past the far wall every time, and the ray leaves unscattered.
    const ConstantMedium thin{&cloud, 0.001_f, nullptr};

    Sampler sampler = make_sampler(34);
    int interactions = 0;

    for (int i = 0; i < 256; ++i) {
        HitRecord rec;
        if (thin.sample_interaction(forward, visible, sampler, rec)) ++interactions;
    }

    // A handful of hits is expected; anything like half of them means the
    // comparison against the segment length is inverted or scaled wrongly.
    REQUIRE(interactions < 20);
}

TEST_CASE("a dense medium scatters immediately", "[geometry][medium]") {
    const ConstantMedium dense{&cloud, 50.0_f, nullptr};

    Sampler sampler = make_sampler(35);

    for (int i = 0; i < 64; ++i) {
        HitRecord rec;
        REQUIRE(dense.sample_interaction(forward, visible, sampler, rec));

        // Just past the entry point, never before it: a scattering event outside
        // the boundary would light up fog where there is none.
        REQUIRE(rec.t >= 5.0_f);
        REQUIRE(rec.t < 5.5_f);
    }
}

TEST_CASE("the mean free path is the reciprocal of the density", "[geometry][medium]") {
    constexpr Float density = 1.0_f;
    const ConstantMedium medium{&cloud, density, nullptr};

    Sampler sampler = make_sampler(36);

    constexpr int samples = 4096;
    double total = 0.0;
    int hits = 0;

    for (int i = 0; i < samples; ++i) {
        HitRecord rec;
        if (medium.sample_interaction(forward, visible, sampler, rec)) {
            total += widen(rec.t) - 5.0;
            ++hits;
        }
    }

    // The transform is the inverse CDF of an exponential distribution, so the
    // mean distance is 1/density. Truncation at the far wall is 20 mean paths
    // away and contributes nothing. The margin is several standard errors wide:
    // this is checking that the distribution has the right shape, not that a
    // particular seed produced a particular number.
    REQUIRE(hits > samples - 10);
    const double mean = total / static_cast<double>(hits);
    REQUIRE(mean > 0.9);
    REQUIRE(mean < 1.1);
}

TEST_CASE("an unbounded boundary carries no medium", "[geometry][medium]") {
    // A quad is a surface, not a volume: the ray enters and never comes out.
    const Quad open{Point3(-5, -5, 5), Vec3(10, 0, 0), Vec3(0, 10, 0), nullptr};
    const ConstantMedium impossible{&open, 1.0_f, nullptr};

    Sampler sampler = make_sampler(37);
    HitRecord rec;

    // Two hits are required before any distance is sampled, so an open boundary
    // is silently ignored rather than producing fog that fills the world.
    REQUIRE_FALSE(impossible.sample_interaction(forward, visible, sampler, rec));

    // And a ray that misses the boundary entirely stops at the first test.
    const ConstantMedium normal{&cloud, 1.0_f, nullptr};
    REQUIRE_FALSE(normal.sample_interaction(Ray(Point3(50, 0, 0), Vec3(0, 0, 1)), visible, sampler, rec));
}

TEST_CASE("the caller's interval clips the segment", "[geometry][medium]") {
    const ConstantMedium medium{&cloud, 50.0_f, nullptr};
    Sampler sampler = make_sampler(38);
    HitRecord rec;

    // Entirely before the volume: nothing to sample.
    REQUIRE_FALSE(medium.sample_interaction(forward, Interval(0.001_f, 3.0_f), sampler, rec));

    // Entirely past it.
    REQUIRE_FALSE(medium.sample_interaction(forward, Interval(30.0_f, pt::infinity), sampler, rec));

    // Clipped to a sliver at the near end: at this density the event lands inside
    // it. The clipping is what lets the integrator limit a medium to the part of
    // the segment before the nearest surface.
    REQUIRE(medium.sample_interaction(forward, Interval(0.001_f, 5.2_f), sampler, rec));
    REQUIRE(rec.t <= 5.2_f);
}

TEST_CASE("a ray starting inside begins scattering at once", "[geometry][medium]") {
    const ConstantMedium medium{&cloud, 1.0_f, nullptr};

    // Origin at the centre of the volume, and an interval that reaches behind it.
    // The entry distance is negative there and gets pulled up to the origin, so a
    // camera parked inside the fog sees fog from where it stands rather than
    // sampling a point behind itself.
    Sampler sampler = make_sampler(39);
    HitRecord rec;
    REQUIRE(medium.sample_interaction(Ray(Point3(0, 0, 15), Vec3(0, 0, 1)), Interval::universe, sampler, rec));

    REQUIRE(rec.t >= 0.0_f);
    REQUIRE(rec.t <= 10.0_f);
}
