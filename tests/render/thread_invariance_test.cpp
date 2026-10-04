#include "pt/render/camera.hpp"
#include "pt/render/path_integrator.hpp"
#include "pt/render/renderer.hpp"
#include "pt/scene/scene.hpp"
#include "pt/scene/scene_loader.hpp"
#include "pt/util/stats.hpp"
#include "pt/util/thread_pool.hpp"
#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <cstdint>
#include <filesystem>
#include <string>

// What a render produces must not depend on how many threads produced it. The
// renderer seeds every sample from its pixel and pass alone, so the work each
// sample does is fixed; only who does it varies. These cases hold that claim
// against the real pipeline: a shipped scene, its BVH, the path integrator,
// the renderer and the pool.

namespace {

// PT_SCENES_DIR is injected by tests/CMakeLists.txt.
const std::filesystem::path scenes_dir{PT_SCENES_DIR};

// Small enough to render many times in a Debug build, large enough that every
// thread count below finds work for every thread at the smallest tile size.
constexpr int image_size = 24;
constexpr int samples_per_pixel = 4;

// Renders `scene` once on `workers` pool threads plus the caller, and returns
// that render's counters. The pool is created here, so its threads are new to
// this render and their slots start empty or freshly reset.
[[nodiscard]] pt::TraversalStats count_render(const pt::Scene& scene, unsigned workers, int tile_size) {
    const pt::Camera camera(scene.camera, scene.render.image_width, scene.render.image_height);
    const pt::PathIntegrator integrator(scene.world(), scene.media(), scene.importance_targets(),
                                        scene.render.background, scene.render.max_depth);
    pt::ThreadPool pool(workers);
    const pt::Renderer renderer(camera, integrator, scene.render, pool, tile_size);

    pt::reset_traversal_stats();
    static_cast<void>(renderer.render());

    // render() returns once every pass has finished, so every worker has been
    // synchronized with: the snapshot's precondition holds here.
    return pt::traversal_snapshot();
}

void require_same_counts(const pt::TraversalStats& actual, const pt::TraversalStats& expected) {
    REQUIRE(actual.node_tests == expected.node_tests);
    REQUIRE(actual.leaf_tests == expected.leaf_tests);
    REQUIRE(actual.ray_queries == expected.ray_queries);
}

} // namespace

TEST_CASE("traversal counters do not depend on the thread count", "[render][threads][stats]") {
    // cornell_box nests boxes inside instances and samples a light; cornell_smoke
    // adds media, whose free-flight sampling changes how many queries a path makes.
    const std::string scene_file = GENERATE("cornell_box.json", "cornell_smoke.json");

    // Zero workers is the serial path with a different tile order; seven exceeds
    // the cores of a CI runner, so the threads also interleave by preemption.
    const unsigned workers = GENERATE(0U, 1U, 3U, 7U);

    // One-pixel tiles put neighbouring pixels on different threads in an order
    // that changes from run to run; the default tile is what production uses.
    const int tile_size = GENERATE(1, pt::Renderer::default_tile_size);

    CAPTURE(scene_file, workers, tile_size);

    pt::Scene scene = pt::load_scene(scenes_dir / scene_file);
    scene.render.image_width = image_size;
    scene.render.image_height = image_size;
    scene.render.samples_per_pixel = samples_per_pixel;

    const pt::TraversalStats serial = count_render(scene, 0, pt::Renderer::default_tile_size);
    const pt::TraversalStats parallel = count_render(scene, workers, tile_size);

    if constexpr (pt::stats_enabled) {
        // Every sample traces its primary ray at least, so fewer queries than
        // samples means a thread's share went missing even in the serial render.
        constexpr std::uint64_t samples = std::uint64_t{image_size} * image_size * samples_per_pixel;
        REQUIRE(serial.ray_queries >= samples);
        REQUIRE(serial.node_tests > 0);
        REQUIRE(serial.leaf_tests > 0);
    } else {
        // Compiled out: nothing is counted, on one thread or many.
        require_same_counts(serial, pt::TraversalStats{});
    }

    // Exact, not within a tolerance: the totals are sums of integers that do not
    // depend on the order they were added in.
    require_same_counts(parallel, serial);

    pt::reset_traversal_stats();
}
