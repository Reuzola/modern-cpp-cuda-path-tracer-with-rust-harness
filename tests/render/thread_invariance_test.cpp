#include "pt/math/color.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/scalar.hpp"
#include "pt/render/camera.hpp"
#include "pt/render/film.hpp"
#include "pt/render/integrator.hpp"
#include "pt/render/path_integrator.hpp"
#include "pt/render/renderer.hpp"
#include "pt/scene/scene.hpp"
#include "pt/scene/scene_loader.hpp"
#include "pt/util/stats.hpp"
#include "pt/util/thread_pool.hpp"
#include "support/test_support.hpp"
#include <atomic>
#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <utility>

// What a render produces must not depend on how many threads produced it. The
// renderer seeds every sample from its pixel and pass alone, and every pixel sums
// its samples in pass order, so the arithmetic is fixed; only who does it varies.
// These cases hold that claim against the real pipeline: a shipped scene, its
// BVH, the path integrator, the renderer and the pool.

namespace {

using pt::operator""_f;

// PT_SCENES_DIR is injected by tests/CMakeLists.txt.
const std::filesystem::path scenes_dir{PT_SCENES_DIR};

// Small enough to render many times under a sanitizer, large enough that the
// default tile size still cuts the image into more tiles than one thread takes.
constexpr int image_size = 24;
constexpr int samples_per_pixel = 4;

// Far longer than a worker takes to wake, sanitizer builds included, so only a
// render that never runs on two threads at all waits this long.
constexpr std::chrono::seconds rendezvous_timeout{10};

// Forwards to the real integrator, but holds each thread at its first sample
// until `threads_to_meet` distinct threads are inside the render at once. A small
// render can otherwise be finished by the calling thread before any worker wakes,
// and comparing two serial renders says nothing about threads. Only the choice of
// who renders which tile changes; every sample is traced exactly as without it.
class RendezvousIntegrator final : public pt::Integrator {
public:
    RendezvousIntegrator(const pt::Integrator& inner, std::size_t threads_to_meet)
        : inner_(inner), threads_to_meet_(threads_to_meet) {}

    [[nodiscard]] pt::Color radiance(const pt::Ray& r, pt::Sampler& sampler) const override {
        meet();
        return inner_.radiance(r, sampler);
    }

    // Read after render() has returned, which synchronizes with every thread.
    [[nodiscard]] bool met() const noexcept { return met_.load(std::memory_order_relaxed); }

private:
    const pt::Integrator& inner_;
    std::size_t threads_to_meet_;
    mutable std::mutex mutex_;
    mutable std::condition_variable all_arrived_;
    mutable std::set<std::thread::id> arrived_; // Guarded by mutex_.
    mutable std::atomic<bool> met_{false};

    void meet() const {
        // A shortcut only: once met, later samples skip the lock entirely.
        if (met_.load(std::memory_order_relaxed)) return;

        std::unique_lock lock(mutex_);

        // A thread that already waited, and gave up, must not wait again on every sample.
        if (!arrived_.insert(std::this_thread::get_id()).second) return;

        if (arrived_.size() >= threads_to_meet_) {
            met_.store(true, std::memory_order_relaxed);
            lock.unlock();
            all_arrived_.notify_all();
            return;
        }

        // Holding this thread inside its tile is what makes the others take tiles of their own.
        all_arrived_.wait_for(lock, rendezvous_timeout, [this] { return met_.load(std::memory_order_relaxed); });
    }
};

struct RenderOutcome {
    pt::Film image;
    pt::TraversalStats counters;
    bool threads_met{};
};

// Loads `scene_file` and renders it once on `workers` pool threads plus the caller.
// The pool exists before the scene does, as in the driver, so a stage that is
// later handed the pool - the BVH build, say - is handed this one, and the
// comparison below covers it too.
[[nodiscard]] RenderOutcome render_on(const std::string& scene_file, unsigned workers, int tile_size) {
    pt::ThreadPool pool(workers);

    pt::Scene scene = pt::load_scene(scenes_dir / scene_file);
    scene.render.image_width = image_size;
    scene.render.image_height = image_size;
    scene.render.samples_per_pixel = samples_per_pixel;

    const pt::Camera camera(scene.camera, scene.render.image_width, scene.render.image_height);
    const pt::PathIntegrator integrator(scene.world(), scene.media(), scene.importance_targets(),
                                        scene.render.background, scene.render.max_depth);

    // Two threads must be inside the render at once whenever there are two to be had.
    const RendezvousIntegrator rendezvous(integrator, workers > 0 ? 2 : 1);
    const pt::Renderer renderer(camera, rendezvous, scene.render, pool, tile_size);

    pt::reset_traversal_stats();
    pt::Film image = renderer.render();

    // render() returns once every pass has finished, so every worker has been
    // synchronized with: the snapshot's precondition holds here.
    return RenderOutcome{.image = std::move(image), .counters = pt::traversal_snapshot(), .threads_met = rendezvous.met()};
}

[[nodiscard]] bool has_light(const pt::Film& film) {
    for (int y = 0; y < film.height(); ++y) {
        for (int x = 0; x < film.width(); ++x) {
            const pt::Color c = film.pixel(x, y);
            if (c.r() > 0.0_f || c.g() > 0.0_f || c.b() > 0.0_f) return true;
        }
    }
    return false;
}

void require_same_counts(const pt::TraversalStats& actual, const pt::TraversalStats& expected) {
    REQUIRE(actual.node_tests == expected.node_tests);
    REQUIRE(actual.leaf_tests == expected.leaf_tests);
    REQUIRE(actual.ray_queries == expected.ray_queries);
}

} // namespace

TEST_CASE("a render does not depend on the thread count", "[render][threads]") {
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

    const RenderOutcome serial = render_on(scene_file, 0, pt::Renderer::default_tile_size);
    const RenderOutcome parallel = render_on(scene_file, workers, tile_size);

    // Without these two, an equality below could be an equality of nothing: two
    // black frames, or a "parallel" render the calling thread did on its own.
    REQUIRE(has_light(serial.image));
    REQUIRE(parallel.threads_met);

    if constexpr (pt::stats_enabled) {
        // Every sample traces its primary ray at least, so fewer queries than
        // samples means a thread's share went missing even in the serial render.
        constexpr std::uint64_t samples = std::uint64_t{image_size} * image_size * samples_per_pixel;
        REQUIRE(serial.counters.ray_queries >= samples);
        REQUIRE(serial.counters.node_tests > 0);
        REQUIRE(serial.counters.leaf_tests > 0);
    } else {
        // Compiled out: nothing is counted, on one thread or many.
        require_same_counts(serial.counters, pt::TraversalStats{});
    }

    // Bit for bit, not within a tolerance: every pixel adds the same samples in
    // the same order on any thread count, so not even the last bit may move.
    pt_test::require_bit_identical(parallel.image, serial.image);

    // Exact as well, for a different reason: the totals are sums of integers,
    // which do not depend on the order they were added in.
    require_same_counts(parallel.counters, serial.counters);

    pt::reset_traversal_stats();
}
