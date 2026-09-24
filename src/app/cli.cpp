#include "cli.hpp"
#include "pt/io/image_format.hpp"
#include "pt/scene/scene.hpp"
#include "pt/util/log.hpp"
#include "pt/util/stats.hpp"
#include "pt/util/thread_pool.hpp"
#include <CLI/CLI.hpp>
#include <algorithm>
#include <limits>
#include <map>
#include <optional>
#include <string>
#include <variant>

#ifndef PT_VERSION
#define PT_VERSION "unknown"
#endif

namespace pt {

std::variant<CliOptions, int> parse_command_line(int argc, char** argv) {
    // The one place an ImageFormat enumerator is spelled that the compiler does
    // not check. is_hdr() and make_image_writer() both omit 'default', so a new
    // enumerator breaks their switches at compile time; it will pass silently
    // through here and simply be unreachable from the command line.
    const std::map<std::string, ImageFormat> format_map{
        {"ppm", ImageFormat::ppm},
        {"png", ImageFormat::png},
        {"exr", ImageFormat::exr}};

    // Same caveat as format_map: enumerators spelled where the compiler cannot check them.
    const std::map<std::string, LogLevel> log_level_map{
        {"info", LogLevel::info},
        {"warning", LogLevel::warning},
        {"error", LogLevel::error},
        {"off", LogLevel::off}};

    const CLI::Range positive_int = CLI::Range(1, std::numeric_limits<int>::max());

    CLI::App app{"Physically-based path tracer"};
    app.set_version_flag("--version", PT_VERSION);

    CliOptions opts;

    app.add_option("scene", opts.scene, "scene file to render")->required();

    CLI::Option* output_opt = app.add_option("-o,--output", opts.output, "Output image path")->capture_default_str();

    CLI::Option* format_opt = app.add_option("-f,--format", opts.format, "Output image format")
                                  ->transform(CLI::CheckedTransformer(format_map, CLI::ignore_case))
                                  ->option_text("FORMAT:{ppm, png, exr}")
                                  ->capture_default_str();

    app.add_option("-l,--log-level", opts.log_level, "Logging verbosity")
        ->transform(CLI::CheckedTransformer(log_level_map, CLI::ignore_case))
        ->option_text("LEVEL:{info, warning, error, off}")
        ->capture_default_str();

    CLI::Option* width_opt = app.add_option("-w,--width", opts.width, "image width to render")->check(positive_int);
    CLI::Option* height_opt = app.add_option("-H,--height", opts.height, "image height to render")->check(positive_int);
    width_opt->needs(height_opt);
    height_opt->needs(width_opt);

    app.add_option("-s,--spp", opts.samples_per_pixel, "samples per pixel")->check(positive_int);

    app.add_option("-d,--max-depth", opts.max_depth, "maximum ray bounce depth")->check(positive_int);

    app.add_option("-S,--seed", opts.seed, "random seed");

    if constexpr (stats_enabled) {
        app.add_option("-t,--threads", opts.threads,
                       "threads to render with; instrumented builds count per thread and accept only 1 (default: 1)");
    } else {
        app.add_option("-t,--threads", opts.threads,
                       "threads to render with, including the caller; -1 = all but one (default: all hardware threads)");
    }

    // Benchmark mode writes one JSON record to stdout; diagnostics stay on the log
    // sink (stderr), so a caller can append records without filtering.
    CLI::Option* bench_opt = app.add_flag("-b,--bench", opts.benchmark, "measure timing and stats, write one JSON record to stdout");

    // Repeats are only meaningful for timing: the counters are deterministic, so
    // the instrumented pass runs once.
    app.add_option("-r,--bench-runs", opts.bench_runs, "timed runs per scene; the reported time is the minimum")
        ->check(positive_int)
        ->capture_default_str()
        ->needs(bench_opt);

    // Image output is meaningless in benchmark mode; reject it instead of ignoring it.
    output_opt->excludes(bench_opt);
    format_opt->excludes(bench_opt);

    try {
        app.parse(argc, argv);

        if (opts.threads.has_value() && *opts.threads != -1 && *opts.threads < 1)
            throw CLI::ValidationError("--threads", "must be -1 or at least 1");

        if constexpr (stats_enabled) {
            if (opts.threads.has_value() && *opts.threads != 1)
                throw CLI::ValidationError("--threads", "instrumented builds count per thread and accept only 1");
        }
    } catch (const CLI::ParseError& e) {
        return app.exit(e);
    }
    return opts;
}

void apply_overrides(Scene& scene, const CliOptions& opts) {
    if (opts.width) scene.render.image_width = *opts.width;
    if (opts.height) scene.render.image_height = *opts.height;
    if (opts.samples_per_pixel) scene.render.samples_per_pixel = *opts.samples_per_pixel;
    if (opts.max_depth) scene.render.max_depth = *opts.max_depth;
    if (opts.seed) scene.render.seed = *opts.seed;
}

int resolve_thread_count(std::optional<int> requested) noexcept {
    const int hardware = static_cast<int>(ThreadPool::default_worker_count()) + 1;

    if (!requested.has_value()) return stats_enabled ? 1 : hardware;
    if (*requested == -1) return std::max(hardware - 1, 1);
    return *requested;
}

} // namespace pt
