#include "pt/core/hit_record.hpp"
#include "pt/geometry/constant_medium.hpp"
#include "pt/geometry/mesh.hpp"
#include "pt/geometry/quad.hpp"
#include "pt/geometry/sphere.hpp"
#include "pt/materials/isotropic.hpp"
#include "pt/math/color.hpp"
#include "pt/math/constants.hpp"
#include "pt/math/interval.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/sampler.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include "pt/textures/solid_color.hpp"
#include "support/test_support.hpp"
#include "support/top_level_support.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <optional>

namespace {

using pt::Color;
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
using pt_test::top_level_of;
using pt_test::widen;

const Interval visible{0.0_f, pt::infinity};

// Large on purpose: the segment inside is 20 units long, so a sampled free path
// almost never runs past the far wall and the cases below stay deterministic
// rather than depending on which draw the seed happens to produce.
const Sphere cloud{Point3(0, 0, 15), 10.0_f, nullptr};

const Ray forward{Point3(0, 0, 0), Vec3(0, 0, 1)};

// A closed cube spanning [-1, 1] on every axis: twelve triangles over eight shared corners,
// corner i having bit 0 for x, bit 1 for y and bit 2 for z. Winding does not matter here:
// the triangle test is two-sided and a medium never reads a normal.
[[nodiscard]] pt::MeshData closed_cube() {
    pt::MeshData data;
    for (int i = 0; i < 8; ++i) {
        data.positions.emplace_back((i & 1) != 0 ? 1.0_f : -1.0_f, (i & 2) != 0 ? 1.0_f : -1.0_f,
                                    (i & 4) != 0 ? 1.0_f : -1.0_f);
    }
    data.indices = {0, 1, 3, 0, 3, 2,  // z = -1
                    4, 5, 7, 4, 7, 6,  // z = +1
                    0, 1, 5, 0, 5, 4,  // y = -1
                    2, 3, 7, 2, 7, 6,  // y = +1
                    0, 2, 6, 0, 6, 4,  // x = -1
                    1, 3, 7, 1, 7, 5}; // x = +1
    return data;
}

} // namespace

TEST_CASE("the interaction distance is the exponential free path", "[geometry][medium]") {
    constexpr Float density = 0.5_f;
    const ConstantMedium medium{top_level_of({&cloud}), density, nullptr};

    // The same stream by hand: this pins the transform from a uniform draw to a
    // distance, and pins the number of draws to one at the same time.
    Sampler by_hand = make_sampler(31);
    const Float u = by_hand.next_scalar_positive();
    const Float expected_distance = -std::log(u) / density;

    Sampler sampler = make_sampler(31);
    const std::optional<Float> t = medium.sample_distance(forward, visible, sampler);
    REQUIRE(t);

    // Entry is at 5, and the direction is a unit vector, so distance and t agree.
    require_near(*t, 5.0_f + expected_distance, 1e-3);
    REQUIRE(sampler.next_uint32() == by_hand.next_uint32());
}

TEST_CASE("the free path is a distance, not a ray parameter", "[geometry][medium]") {
    constexpr Float density = 0.5_f;
    const ConstantMedium medium{top_level_of({&cloud}), density, nullptr};

    // Twice as long a direction: entry lands at t = 2.5 rather than 5, and the
    // sampled distance has to be divided by the direction's length before it can
    // be added to it. Getting that division wrong is invisible from the camera,
    // where directions are unit vectors, and wrong for every medium inside a
    // scaled instance.
    Sampler by_hand = make_sampler(32);
    const Float u = by_hand.next_scalar_positive();
    const Float expected_distance = -std::log(u) / density;

    const Ray doubled(Point3(0, 0, 0), Vec3(0, 0, 2));
    Sampler sampler = make_sampler(32);
    const std::optional<Float> t = medium.sample_distance(doubled, visible, sampler);
    REQUIRE(t);
    require_near(*t, 2.5_f + expected_distance / 2.0_f, 1e-3);

    HitRecord rec;
    medium.fill(doubled, *t, rec);
    require_vec_near(rec.p, Point3(0, 0, 5.0_f + expected_distance), 1e-2);
}

TEST_CASE("the record describes a scattering event, not a surface", "[geometry][medium]") {
    const pt::SolidColor grey{Color(0.5_f, 0.5_f, 0.5_f)};
    const pt::Isotropic phase{&grey};
    const ConstantMedium medium{top_level_of({&cloud}), 0.5_f, &phase};

    Sampler sampler = make_sampler(33);
    const std::optional<Float> t = medium.sample_distance(forward, visible, sampler);
    REQUIRE(t);

    // Filled over a record that still holds a surface hit, as a caller's record may: every field has to be
    // the medium's own afterwards, or a surface's margin, side or coordinates would leak into the volume.
    HitRecord rec;
    rec.u = 0.5_f;
    rec.v = 0.75_f;
    rec.spawn_margin = 1.0_f;
    rec.geometric_normal = Vec3(0, 1, 0);
    rec.front_face = false;
    medium.fill(forward, *t, rec);

    REQUIRE(rec.t == *t);
    require_vec_near(rec.p, forward.at(*t));

    // The normal is arbitrary and front_face is unconditionally true: an isotropic phase function scatters over
    // the whole sphere and never consults either. The zero geometric normal is what makes a bounce leave from
    // the sample point itself, with no surface to clear.
    require_vec_near(rec.normal, Vec3(1, 0, 0));
    require_vec_near(rec.geometric_normal, Vec3(0, 0, 0));
    REQUIRE(rec.spawn_margin == 0.0_f);
    REQUIRE(rec.front_face);
    REQUIRE(rec.u == 0.0_f);
    REQUIRE(rec.v == 0.0_f);
    REQUIRE(rec.mat == &phase);
}

TEST_CASE("a thin medium lets rays through", "[geometry][medium]") {
    // Mean free path of a thousand units against a segment of twenty: the sampled
    // distance runs past the far wall every time, and the ray leaves unscattered.
    const ConstantMedium thin{top_level_of({&cloud}), 0.001_f, nullptr};

    Sampler sampler = make_sampler(34);
    int interactions = 0;

    for (int i = 0; i < 256; ++i) {
        if (thin.sample_distance(forward, visible, sampler)) ++interactions;
    }

    // A handful of hits is expected; anything like half of them means the
    // comparison against the segment length is inverted or scaled wrongly.
    REQUIRE(interactions < 20);
}

TEST_CASE("a dense medium scatters immediately", "[geometry][medium]") {
    const ConstantMedium dense{top_level_of({&cloud}), 50.0_f, nullptr};

    Sampler sampler = make_sampler(35);

    for (int i = 0; i < 64; ++i) {
        const std::optional<Float> t = dense.sample_distance(forward, visible, sampler);
        REQUIRE(t);

        // Just past the entry point, never before it: a scattering event outside
        // the boundary would light up fog where there is none.
        REQUIRE(*t >= 5.0_f);
        REQUIRE(*t < 5.5_f);
    }
}

TEST_CASE("the mean free path is the reciprocal of the density", "[geometry][medium]") {
    constexpr Float density = 1.0_f;
    const ConstantMedium medium{top_level_of({&cloud}), density, nullptr};

    Sampler sampler = make_sampler(36);

    constexpr int samples = 4096;
    double total = 0.0;
    int hits = 0;

    for (int i = 0; i < samples; ++i) {
        if (const std::optional<Float> t = medium.sample_distance(forward, visible, sampler)) {
            total += widen(*t) - 5.0;
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
    const ConstantMedium impossible{top_level_of({&open}), 1.0_f, nullptr};

    Sampler sampler = make_sampler(37);

    // Two hits are required before any distance is sampled, so an open boundary
    // is silently ignored rather than producing fog that fills the world.
    REQUIRE_FALSE(impossible.sample_distance(forward, visible, sampler));

    // And a ray that misses the boundary entirely stops at the first test.
    const ConstantMedium normal{top_level_of({&cloud}), 1.0_f, nullptr};
    REQUIRE_FALSE(normal.sample_distance(Ray(Point3(50, 0, 0), Vec3(0, 0, 1)), visible, sampler));
}

TEST_CASE("the caller's interval clips the segment", "[geometry][medium]") {
    const ConstantMedium medium{top_level_of({&cloud}), 50.0_f, nullptr};
    Sampler sampler = make_sampler(38);

    // Entirely before the volume: nothing to sample.
    REQUIRE_FALSE(medium.sample_distance(forward, Interval(0.001_f, 3.0_f), sampler));

    // Entirely past it.
    REQUIRE_FALSE(medium.sample_distance(forward, Interval(30.0_f, pt::infinity), sampler));

    // Clipped to a sliver at the near end: at this density the event lands inside
    // it. The clipping is what lets the integrator limit a medium to the part of
    // the segment before the nearest surface.
    const std::optional<Float> t = medium.sample_distance(forward, Interval(0.001_f, 5.2_f), sampler);
    REQUIRE(t);
    REQUIRE(*t <= 5.2_f);
}

TEST_CASE("a ray starting inside begins scattering at once", "[geometry][medium]") {
    const ConstantMedium medium{top_level_of({&cloud}), 1.0_f, nullptr};

    // Origin at the centre of the volume, and an interval that reaches behind it.
    // The entry distance is negative there and gets pulled up to the origin, so a
    // camera parked inside the fog sees fog from where it stands rather than
    // sampling a point behind itself.
    Sampler sampler = make_sampler(39);
    const std::optional<Float> t = medium.sample_distance(Ray(Point3(0, 0, 15), Vec3(0, 0, 1)), Interval::universe, sampler);
    REQUIRE(t);
    REQUIRE(*t >= 0.0_f);
    REQUIRE(*t <= 10.0_f);
}

TEST_CASE("a medium far from the origin still finds its exit", "[geometry][constant_medium]") {
    // The exit search used to start a fixed 1e-4 past the entry, which is below
    // the resolution of t out here and was doing nothing; the strict interval is
    // what actually separates the two hits.
    constexpr Float scale = 100'000.0_f;
    const Sphere boundary{Point3(scale, scale, scale), 1.0_f, nullptr};
    const ConstantMedium medium{top_level_of({&boundary}), 50.0_f, nullptr};

    const Point3 origin(scale, scale, scale - 10.0_f);
    const Ray r(origin, Vec3(0, 0, 1));

    Sampler sampler = make_sampler(501);
    const std::optional<Float> t = medium.sample_distance(r, visible, sampler);
    REQUIRE(t);
    REQUIRE(*t > 9.0_f);
    REQUIRE(*t < 11.0_f);
}

TEST_CASE("a mesh boundary holds its medium for a ray that starts inside", "[geometry][medium]") {
    const pt::Mesh mesh(closed_cube(), nullptr);
    const ConstantMedium medium{top_level_of({&mesh}), 50.0_f, nullptr};

    Sampler sampler = make_sampler(40);

    // Off the faces' diagonals, so neither crossing lands on an edge two triangles share.
    constexpr Float x = 0.2_f;
    constexpr Float y = -0.3_f;

    // From outside: entry at t = 4, and at this density the event lands just past it. This
    // half passes with or without the fix: the boundary itself is sound, so a failure below
    // can only be the search behind the origin.
    for (int i = 0; i < 16; ++i) {
        const std::optional<Float> t = medium.sample_distance(Ray(Point3(x, y, -5.0_f), Vec3(0, 0, 1)), visible, sampler);
        REQUIRE(t);
        REQUIRE(*t >= 4.0_f);
        REQUIRE(*t < 4.5_f);
    }

    // From inside, where every bounce after the first scattering event starts. The entry is
    // behind the origin at t = -1.1; unless the boundary search reports it, the exit is taken
    // for the entry, no second crossing exists, and the medium vanishes for this ray.
    for (int i = 0; i < 16; ++i) {
        const std::optional<Float> t = medium.sample_distance(Ray(Point3(x, y, 0.1_f), Vec3(0, 0, 1)), visible, sampler);
        REQUIRE(t);
        REQUIRE(*t >= 0.0_f);
        REQUIRE(*t <= 0.9_f);
    }
}
