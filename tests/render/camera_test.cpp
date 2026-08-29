#include "pt/math/ray.hpp"
#include "pt/math/sampler.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include "pt/render/camera.hpp"
#include "pt/scene/scene.hpp"
#include "support/test_support.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cmath>

namespace {

using pt::Camera;
using pt::CameraSettings;
using pt::Float;
using pt::Point3;
using pt::Ray;
using pt::Sampler;
using pt::Vec3;
using pt::unit_vector;
using pt::operator""_f;
using pt_test::make_sampler;
using pt_test::require_near;
using pt_test::require_vec_near;

// Looking down -z from the origin, with a 90 degree vertical field of view and
// the focal plane one unit away. The viewport is then exactly two units tall,
// which makes every expectation below an exact number rather than a tolerance.
constexpr CameraSettings straight_ahead{
    .vfov = 90.0_f,
    .lookfrom = Point3{0, 0, 0},
    .lookat = Point3{0, 0, -1},
    .vup = Vec3{0, 1, 0},
    .defocus_angle = 0.0_f,
    .focus_dist = 1.0_f,
};

const Vec3 no_offset{0, 0, 0};

} // namespace

TEST_CASE("the pixel grid lands where the geometry says", "[render][camera]") {
    const Camera camera(straight_ahead, 2, 2);
    Sampler sampler = make_sampler(91);

    // A 2x2 image over a 2x2 viewport: one unit per pixel. The corner of pixel
    // (0, 0) sits half a pixel in from the top left of the viewport.
    require_vec_near(camera.generate_ray(0, 0, no_offset, sampler).direction(), Vec3(-0.5_f, 0.5_f, -1.0_f));
    require_vec_near(camera.generate_ray(1, 0, no_offset, sampler).direction(), Vec3(0.5_f, 0.5_f, -1.0_f));
    require_vec_near(camera.generate_ray(0, 1, no_offset, sampler).direction(), Vec3(-0.5_f, -0.5_f, -1.0_f));
    require_vec_near(camera.generate_ray(1, 1, no_offset, sampler).direction(), Vec3(0.5_f, -0.5_f, -1.0_f));

    // Row zero is the top of the image and y decreases downwards, matching the
    // film's own indexing. Getting this backwards produces a vertically mirrored
    // render that looks entirely plausible until something is upside down.
    const Ray top = camera.generate_ray(0, 0, no_offset, sampler);
    const Ray bottom = camera.generate_ray(0, 1, no_offset, sampler);
    REQUIRE(top.direction().y() > bottom.direction().y());
}

TEST_CASE("the pixel offset moves the sample within its pixel", "[render][camera]") {
    const Camera camera(straight_ahead, 2, 2);
    Sampler sampler = make_sampler(92);

    // The offset is in pixel units and spans [-0.5, 0.5], so half a pixel takes
    // the sample from a pixel's corner to the centre of the image here.
    require_vec_near(camera.generate_ray(0, 0, Vec3(0.5_f, 0.5_f, 0), sampler).direction(), Vec3(0, 0, -1));

    // Stratified jitter is the renderer's job; the camera only applies what it is
    // handed, which is what keeps the sampling pattern out of this class.
    require_vec_near(camera.generate_ray(0, 0, Vec3(-0.5_f, -0.5_f, 0), sampler).direction(), Vec3(-1.0_f, 1.0_f, -1.0_f));
}

TEST_CASE("the horizontal field of view follows the image shape", "[render][camera]") {
    // Twice as wide, same height: the viewport widens and the vertical extent is
    // untouched. There is no stored aspect ratio - it is recomputed from the
    // image dimensions, so a resized render reframes instead of stretching.
    const Camera wide(straight_ahead, 4, 2);
    Sampler sampler = make_sampler(93);

    require_vec_near(wide.generate_ray(0, 0, no_offset, sampler).direction(), Vec3(-1.5_f, 0.5_f, -1.0_f));
    require_vec_near(wide.generate_ray(3, 1, no_offset, sampler).direction(), Vec3(1.5_f, -0.5_f, -1.0_f));
}

TEST_CASE("a narrower field of view zooms in", "[render][camera]") {
    // tan(26.565 degrees) is 0.5, so the viewport is half as tall and the same
    // pixel looks at a point half as far off-axis.
    constexpr CameraSettings narrow{
        .vfov = 53.13010_f,
        .lookfrom = Point3{0, 0, 0},
        .lookat = Point3{0, 0, -1},
        .vup = Vec3{0, 1, 0},
        .defocus_angle = 0.0_f,
        .focus_dist = 1.0_f,
    };

    const Camera camera(narrow, 2, 2);
    Sampler sampler = make_sampler(94);

    require_vec_near(camera.generate_ray(0, 0, no_offset, sampler).direction(), Vec3(-0.25_f, 0.25_f, -1.0_f), 1e-3);
}

TEST_CASE("the focus distance changes no framing", "[render][camera]") {
    constexpr CameraSettings far_focus{
        .vfov = 90.0_f,
        .lookfrom = Point3{0, 0, 0},
        .lookat = Point3{0, 0, -1},
        .vup = Vec3{0, 1, 0},
        .defocus_angle = 0.0_f,
        .focus_dist = 7.0_f,
    };

    const Camera near_camera(straight_ahead, 3, 3);
    const Camera far_camera(far_focus, 3, 3);
    Sampler sampler = make_sampler(95);

    // The viewport scales with the focus distance, so the rays through a given
    // pixel are the same ray at a different length. Pulling focus must not
    // reframe the shot - it only decides which depth is sharp.
    for (int y = 0; y < 3; ++y) {
        for (int x = 0; x < 3; ++x) {
            const Vec3 near_direction = near_camera.generate_ray(x, y, no_offset, sampler).direction();
            const Vec3 far_direction = far_camera.generate_ray(x, y, no_offset, sampler).direction();
            require_vec_near(unit_vector(near_direction), unit_vector(far_direction), 1e-4);
        }
    }
}

TEST_CASE("an arbitrary orientation keeps its up vector", "[render][camera]") {
    constexpr CameraSettings from_the_side{
        .vfov = 90.0_f,
        .lookfrom = Point3{5, 0, 0},
        .lookat = Point3{0, 0, 0},
        .vup = Vec3{0, 1, 0},
        .defocus_angle = 0.0_f,
        .focus_dist = 5.0_f,
    };

    const Camera camera(from_the_side, 2, 2);
    Sampler sampler = make_sampler(96);

    // The centre of the image points at the target, wherever the camera stands.
    const Ray centre = camera.generate_ray(0, 0, Vec3(0.5_f, 0.5_f, 0), sampler);
    require_vec_near(centre.origin(), Point3(5, 0, 0));
    require_vec_near(unit_vector(centre.direction()), Vec3(-1, 0, 0), 1e-4);

    // Up on the image is up in the world: the basis is built from vup, not from
    // a fixed world axis, so the horizon stays level under any yaw.
    const Ray top = camera.generate_ray(0, 0, no_offset, sampler);
    const Ray bottom = camera.generate_ray(0, 1, no_offset, sampler);
    REQUIRE(top.direction().y() > bottom.direction().y());
}

TEST_CASE("a pinhole camera draws once, for the shutter instant", "[render][camera]") {
    const Camera camera(straight_ahead, 2, 2);

    Sampler used = make_sampler(97);
    const Ray ray = camera.generate_ray(0, 0, no_offset, used);

    Sampler expected = make_sampler(97);
    const Float expected_time = expected.next_scalar();

    // Exactly one draw, and it is the time. The lens is skipped rather than
    // sampled with a zero radius, so a scene without depth of field consumes a
    // different amount of the stream than one with it - which is why turning
    // defocus on changes the noise pattern everywhere, not just at the edges.
    require_near(ray.time(), expected_time);
    REQUIRE(used.next_uint32() == expected.next_uint32());

    // Every ray leaves the same point.
    require_vec_near(ray.origin(), Point3(0, 0, 0));
}

TEST_CASE("a non-positive aperture is a pinhole", "[render][camera]") {
    constexpr CameraSettings negative_aperture{
        .vfov = 90.0_f,
        .lookfrom = Point3{0, 0, 0},
        .lookat = Point3{0, 0, -1},
        .vup = Vec3{0, 1, 0},
        .defocus_angle = -5.0_f,
        .focus_dist = 1.0_f,
    };

    const Camera camera(negative_aperture, 2, 2);
    Sampler sampler = make_sampler(98);

    // Zero is the documented way to switch depth of field off, and anything below
    // it means the same thing rather than a mirrored aperture.
    require_vec_near(camera.generate_ray(0, 0, no_offset, sampler).origin(), Point3(0, 0, 0));
}

TEST_CASE("a real aperture spreads the origin over a disk", "[render][camera]") {
    constexpr CameraSettings with_aperture{
        .vfov = 90.0_f,
        .lookfrom = Point3{0, 0, 0},
        .lookat = Point3{0, 0, -1},
        .vup = Vec3{0, 1, 0},
        .defocus_angle = 20.0_f,
        .focus_dist = 1.0_f,
    };

    const Camera camera(with_aperture, 2, 2);
    Sampler sampler = make_sampler(99);

    // tan(10 degrees) at a focus distance of one.
    constexpr Float radius = 0.1763270_f;

    Float largest_offset{0};

    for (int i = 0; i < 128; ++i) {
        const Ray ray = camera.generate_ray(0, 0, no_offset, sampler);

        // The origin moves across the lens, but the ray still passes through the
        // same point on the focal plane: origin + direction is that point, by
        // construction. That is the whole mechanism - everything at the focus
        // distance stays sharp no matter where on the lens the ray started, and
        // everything nearer or further smears.
        require_vec_near(ray.origin() + ray.direction(), Point3(-0.5_f, 0.5_f, -1.0_f), 1e-4);

        // Inside the aperture, and flat: the lens has no thickness.
        const Vec3 offset = ray.origin() - Point3(0, 0, 0);
        require_near(offset.z(), 0.0_f, 1e-4);
        REQUIRE(offset.length() <= radius + 1e-4_f);

        largest_offset = std::fmax(largest_offset, offset.length());
    }

    // And it does use the aperture rather than collapsing to the centre.
    REQUIRE(largest_offset > radius * 0.5_f);
}
