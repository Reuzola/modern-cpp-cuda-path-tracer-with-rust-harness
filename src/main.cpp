#include "app/benchmark.hpp"
#include "app/cli.hpp"
#include "pt/geometry/bvh.hpp"
#include "pt/io/image_format.hpp"
#include "pt/io/image_writer.hpp"
#include "pt/post/tonemap.hpp"
#include "pt/render/camera.hpp"
#include "pt/render/film.hpp"
#include "pt/render/path_integrator.hpp"
#include "pt/render/progress.hpp"
#include "pt/render/renderer.hpp"
#include "pt/scene/scene.hpp"
#include "pt/scene/scene_error.hpp"
#include "pt/scene/scene_loader.hpp"
#include "pt/util/log.hpp"
#include "pt/util/stats.hpp"
#include <chrono>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <ratio>
#include <system_error>
#include <utility>
#include <variant>
#include <vector>

namespace {

[[nodiscard]] int render_scene(const pt::Scene& scene, const pt::CliOptions& opts) {
    const int image_width = scene.render.image_width;
    const int image_height = scene.render.image_height;

    const pt::Camera camera(scene.camera, image_width, image_height);
    const pt::PathIntegrator integrator(scene.world(), scene.media(), scene.importance_targets(), scene.render.background, scene.render.max_depth);
    const pt::Renderer renderer(camera, integrator, scene.render);
    pt::ConsoleProgressReporter reporter;

    std::error_code ec;
    const std::filesystem::path parent = opts.output.parent_path();
    if (!parent.empty()) std::filesystem::create_directories(parent, ec);
    if (ec) {
        pt::log_error("Could not create directory '{}': {}", parent.string(), ec.message());
        return EXIT_FAILURE;
    }

    const pt::ProgressCallback progress =
        pt::should_log(pt::LogLevel::info) ? pt::ProgressCallback(std::ref(reporter)) : pt::ProgressCallback{};

    const auto start = std::chrono::steady_clock::now();
    const pt::Film film = renderer.render(std::ref(progress));
    const auto end = std::chrono::steady_clock::now();

    std::optional<pt::Film> display;
    if (!pt::is_hdr(opts.format)) {
        display = pt::tone_map(film, scene.render.tone_map);
    }
    const pt::Film& image = display ? *display : film;

    const std::unique_ptr<pt::ImageWriter> writer = pt::make_image_writer(opts.format);
    if (!writer->write(image, opts.output)) {
        pt::log_error("Could not write image '{}'", opts.output.string());
        return EXIT_FAILURE;
    }

    const std::chrono::duration<double> elapsed = end - start;

    pt::log_info("Render time: {:.2f}s", elapsed.count());

    if constexpr (pt::stats_enabled) {
        const pt::TraversalStats& stats = pt::traversal_stats;

        if (stats.ray_queries > 0) {

            const double node_ratio = static_cast<double>(stats.node_tests) / static_cast<double>(stats.ray_queries);
            const double leaf_ratio = static_cast<double>(stats.leaf_tests) / static_cast<double>(stats.ray_queries);

            pt::log_info("BVH traversal: {:.1f} node tests/ray, {:.1f} leaf tests/ray ({} ray queries)", node_ratio, leaf_ratio, stats.ray_queries);
        }
    }

    return EXIT_SUCCESS;
}

// Benchmark path: renders repeatedly without writing an image. The camera,
// integrator and renderer are built once, so setup is outside every timing.
[[nodiscard]] int run_benchmark(const pt::Scene& scene, const pt::CliOptions& opts) {
    const int image_width = scene.render.image_width;
    const int image_height = scene.render.image_height;

    const pt::Camera camera(scene.camera, image_width, image_height);
    const pt::PathIntegrator integrator(scene.world(), scene.media(), scene.importance_targets(), scene.render.background, scene.render.max_depth);
    const pt::Renderer renderer(camera, integrator, scene.render);

    std::vector<double> render_seconds;
    render_seconds.reserve(static_cast<std::size_t>(opts.bench_runs));

    // Keeps the rendered frame observably used, so LTO cannot elide the call under measurement.
    // Plain store, never a compound assignment: volatile += is deprecated in C++20.
    volatile double sink{};

    for (int i = 0; i < opts.bench_runs; ++i) {
        // Counters accumulate; reset each run so the snapshot describes a single render.
        pt::reset_traversal_stats();

        const auto start = std::chrono::steady_clock::now();
        const pt::Film film = renderer.render();
        const auto end = std::chrono::steady_clock::now();

        sink = static_cast<double>(film.pixel(0, 0).r());
        render_seconds.push_back(std::chrono::duration<double>(end - start).count());
    }

    static_cast<void>(sink);

    std::optional<pt::TraversalStats> traversal;
    if constexpr (pt::stats_enabled) traversal = pt::traversal_snapshot();

    pt::BenchmarkRecord record{
        .scene = opts.scene.string(),
        .timestamp = pt::utc_timestamp(),
        .host = pt::detect_host(),
        .build = pt::detect_build(),
        .image_width = image_width,
        .image_height = image_height,
        .samples_per_pixel = renderer.samples_per_pixel(),
        .max_depth = scene.render.max_depth,
        .seed = scene.render.seed,
        .render_seconds = std::move(render_seconds),
        .bvh = scene.bvh_stats(),
        .traversal = traversal,
    };

    pt::write_record(record, std::cout);
    return EXIT_SUCCESS;
}

[[nodiscard]] int run(int argc, char** argv) {
    const std::variant<pt::CliOptions, int> parsed = pt::parse_command_line(argc, argv);
    if (const int* exit_code = std::get_if<int>(&parsed)) return *exit_code;

    const pt::CliOptions& opts = std::get<pt::CliOptions>(parsed);

    pt::set_log_level(opts.log_level);

    std::optional<pt::Scene> scene;
    try {
        scene = pt::load_scene(opts.scene);
    } catch (const pt::SceneError& e) {
        pt::log_error("{}", e.what());
        return EXIT_FAILURE;
    }

    pt::apply_overrides(*scene, opts);

    const pt::BvhStats& stats = scene->bvh_stats();
    pt::log_info(
        "BVH: {} trees, {} nodes, {} leaves, max depth {}, built in {:.3f} ms",
        stats.bvh_count,
        stats.node_count,
        stats.leaf_count,
        stats.max_depth,
        std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(stats.build_time).count());

    if (opts.benchmark) return run_benchmark(*scene, opts);

    return render_scene(*scene, opts);
}

} // namespace

int main(int argc, char** argv) {
    try {
        return run(argc, argv);
    } catch (const std::exception& e) {
        pt::log_error("{}", e.what());
        return EXIT_FAILURE;
    } catch (...) {
        pt::log_error("Unknown error");
        return EXIT_FAILURE;
    }
}
