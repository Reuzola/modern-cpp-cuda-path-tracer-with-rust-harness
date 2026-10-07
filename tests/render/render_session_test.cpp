#include "pt/geometry/sphere.hpp"
#include "pt/materials/diffuse_light.hpp"
#include "pt/materials/lambertian.hpp"
#include "pt/math/color.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include "pt/render/camera.hpp"
#include "pt/render/film.hpp"
#include "pt/render/path_integrator.hpp"
#include "pt/render/render_session.hpp"
#include "pt/render/renderer.hpp"
#include "pt/scene/scene.hpp"
#include "pt/textures/solid_color.hpp"
#include "pt/util/thread_pool.hpp"
#include "support/test_support.hpp"
#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <chrono>
#include <cstdint>
#include <optional>
#include <thread>
#include <utility>
#include <vector>

// Same discipline as the pool tests: nothing here asserts from another thread.
// The session is driven from the test thread exactly as a frontend would drive it.

namespace {

using pt::operator""_f;

constexpr int width = 48;
constexpr int height = 32;

// A lit diffuse ball: every ray does real work - a BVH hit, a scatter, a light
// sample - so a pass takes long enough for an update to land inside one.
[[nodiscard]] pt::Scene make_scene() {
    pt::Scene scene;
    scene.render.image_width = width;
    scene.render.image_height = height;
    scene.render.samples_per_pixel = 16;
    scene.render.max_depth = 6;
    scene.render.seed = pt_test::base_seed;
    scene.render.background = pt::Color(0.2_f, 0.25_f, 0.3_f);
    scene.camera = pt::CameraSettings{
        .vfov = 60.0_f,
        .lookfrom = pt::Point3{0, 0, 3},
        .lookat = pt::Point3{0, 0, 0},
        .vup = pt::Vec3{0, 1, 0},
        .defocus_angle = 0.0_f,
        .focus_dist = 3.0_f,
    };

    const auto* grey = scene.create_texture<pt::SolidColor>(0.7_f, 0.7_f, 0.7_f);
    const auto* glow = scene.create_texture<pt::SolidColor>(4.0_f, 4.0_f, 4.0_f);
    const auto* diffuse = scene.create_material<pt::Lambertian>(grey);
    const auto* light = scene.create_material<pt::DiffuseLight>(glow);

    const auto* ball = scene.create_object<pt::Sphere>(pt::Point3{0, 0, 0}, 1.0_f, diffuse);
    const auto* lamp = scene.create_object<pt::Sphere>(pt::Point3{0, 2.5_f, 1}, 0.5_f, light);
    scene.add_object(ball);
    scene.add_object(lamp);
    scene.add_importance_target(lamp);
    scene.build_bvh();
    return scene;
}

[[nodiscard]] pt::SessionParameters parameters_of(const pt::Scene& scene) {
    return {.camera = scene.camera, .max_depth = scene.render.max_depth, .samples_per_pixel = scene.render.samples_per_pixel};
}

// What the offline driver would produce for the same parameters: a fresh camera,
// integrator and renderer, one thread, every pass in one call.
[[nodiscard]] pt::Film offline_render(const pt::Scene& scene, const pt::SessionParameters& params) {
    pt::RenderSettings settings = scene.render;
    settings.samples_per_pixel = params.samples_per_pixel;

    const pt::Camera camera(params.camera, settings.image_width, settings.image_height);
    const pt::PathIntegrator integrator(scene.world(), scene.media(), scene.importance_targets(), settings.background, params.max_depth);
    pt::ThreadPool serial(0);
    return pt::Renderer(camera, integrator, settings, serial).render();
}

struct Converged {
    pt::Film image;
    pt::FrameInfo info;
};

// Polls like a frontend's frame loop until the image for `generation` is complete.
// Returning at all proves the final image is published even when the session
// finished it while the mailbox was still full. Empty after the deadline: a hang
// becomes a failure instead of a stuck CI job.
[[nodiscard]] std::optional<Converged> wait_until_converged(pt::RenderSession& session, std::uint64_t generation) {
    pt::Film image(width, height);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(120);

    while (std::chrono::steady_clock::now() < deadline) {
        if (const std::optional<pt::FrameInfo> info = session.take_frame(image)) {
            if (info->generation == generation && info->sample_count == info->target_spp) {
                return Converged{.image = std::move(image), .info = *info};
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return std::nullopt;
}

} // namespace

TEST_CASE("a converged session shows exactly the offline render", "[render][render_session]") {
    const unsigned workers = GENERATE(0U, 3U);
    CAPTURE(workers);

    const pt::Scene scene = make_scene();
    pt::ThreadPool pool(workers);
    pt::RenderSession session(scene, pool, parameters_of(scene));

    const std::optional<Converged> result = wait_until_converged(session, 0);
    REQUIRE(result.has_value());

    // The viewer and the command line must agree to the bit: a screenshot taken in
    // the viewer is then the image the driver would have written.
    REQUIRE(result->info.sample_count == 16);
    REQUIRE(result->info.target_spp == 16);
    pt_test::require_bit_identical(result->image, offline_render(scene, parameters_of(scene)));
}

TEST_CASE("a session converges to its last update as if the others never happened", "[render][render_session]") {
    const unsigned workers = GENERATE(0U, 3U);
    CAPTURE(workers);

    const pt::Scene scene = make_scene();
    pt::ThreadPool pool(workers);
    pt::RenderSession session(scene, pool, parameters_of(scene));

    // Updates arrive faster than passes complete, so most of them cancel a pass in
    // flight and leave a partial one behind. Depth and spp change too, not just the
    // camera: every parameter must reach the renderer.
    pt::SessionParameters params = parameters_of(scene);
    std::uint64_t last = 0;
    for (int k = 1; k <= 6; ++k) {
        params.camera.lookfrom = pt::Point3{0.2_f * static_cast<pt::Float>(k), 0, 3};
        params.max_depth = 3 + (k % 3);
        params.samples_per_pixel = (k % 2 == 0) ? 9 : 16;
        last = session.update(params);
        std::this_thread::sleep_for(std::chrono::microseconds(300));
    }
    REQUIRE(last == 6);

    const std::optional<Converged> result = wait_until_converged(session, last);
    REQUIRE(result.has_value());

    // Nothing from a cancelled pass or an earlier camera may survive the restart.
    pt_test::require_bit_identical(result->image, offline_render(scene, params));
}

TEST_CASE("no frame taken after an update shows older parameters", "[render][render_session]") {
    const unsigned workers = GENERATE(0U, 3U);
    CAPTURE(workers);

    const pt::Scene scene = make_scene();
    pt::ThreadPool pool(workers);
    pt::RenderSession session(scene, pool, parameters_of(scene));

    pt::Film image(width, height);
    std::vector<std::uint64_t> stale;
    pt::SessionParameters params = parameters_of(scene);

    for (int k = 1; k <= 8; ++k) {
        params.camera.lookfrom = pt::Point3{0.1_f * static_cast<pt::Float>(k), 0, 3};
        const std::uint64_t current = session.update(params);

        for (int poll = 0; poll < 20; ++poll) {
            if (const std::optional<pt::FrameInfo> info = session.take_frame(image); info && info->generation != current) {
                stale.push_back(info->generation);
            }
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    }

    // update() empties the mailbox, and an image whose resolve overlapped the update
    // is dropped. A frontend that measures edit-to-screen latency by generation
    // relies on this: an old frame would count as the new one's arrival.
    REQUIRE(stale.empty());
}

TEST_CASE("a session with an unreachable target can still be destroyed", "[render][render_session]") {
    pt::Scene scene = make_scene();
    scene.render.samples_per_pixel = 1'000'000;
    pt::ThreadPool pool(3);

    {
        pt::RenderSession session(scene, pool, parameters_of(scene));

        // Wait for the first image, so the destructor meets a session that is
        // rendering rather than one still starting up.
        pt::Film image(width, height);
        while (!session.take_frame(image)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    // Can only fail by hanging: the target is a million passes away, so the
    // destructor returns only if it both cancels the pass and ends the loop.
    SUCCEED("the session stopped mid-render");
}

TEST_CASE("a session counts the pool's workers and its own thread", "[render][render_session]") {
    const unsigned workers = GENERATE(0U, 1U, 3U);
    const pt::Scene scene = make_scene();
    pt::ThreadPool pool(workers);
    const pt::RenderSession session(scene, pool, parameters_of(scene));

    // The frontend's thread draws, it does not render; benchmark-style records
    // must not count it.
    REQUIRE(session.thread_count() == static_cast<int>(workers) + 1);
}
