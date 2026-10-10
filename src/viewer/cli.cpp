#include "viewer/cli.hpp"
#include "pt/scene/scene.hpp"
#include "pt/util/log.hpp"
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

std::variant<ViewerOptions, int> parse_viewer_command_line(int argc, char** argv) {
    const std::map<std::string, LogLevel> log_level_map{
        {"info", LogLevel::info},
        {"warning", LogLevel::warning},
        {"error", LogLevel::error},
        {"off", LogLevel::off}};

    const CLI::Range positive_int = CLI::Range(1, std::numeric_limits<int>::max());

    CLI::App app{"Interactive path tracer viewer"};
    app.set_version_flag("--version", PT_VERSION);

    ViewerOptions opts;

    app.add_option("scene", opts.scene, "scene file to render")->required()->check(CLI::ExistingFile);
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
    app.add_option("-t,--threads", opts.threads,
                   "render threads, counting the one that drives the passes; -1 = all but one (default: all hardware threads but one, left to the UI)");
    app.add_option("-m,--measure-images", opts.measure_images,
                   "drive the camera until N new images have been shown, then hold it for up to N more; write one frame-time record to stdout and exit")
        ->check(positive_int);
    app.add_option("-u,--ui-scale", opts.ui_scale, "UI scale factor (default: platform content scale)")->check(CLI::Range(0.5F, 4.0F));
    app.add_option("-f,--max-fps", opts.max_fps, "frame rate cap; 0 = uncapped (default: the display's refresh rate)")
        ->check(CLI::Range(0, 1000));

    try {
        app.parse(argc, argv);

        if (opts.threads.has_value() && *opts.threads != -1 && *opts.threads < 1)
            throw CLI::ValidationError("--threads", "must be -1 or at least 1");
    } catch (const CLI::ParseError& e) {
        return app.exit(e);
    }
    return opts;
}

void apply_overrides(Scene& scene, const ViewerOptions& opts) {
    if (opts.width) scene.render.image_width = *opts.width;
    if (opts.height) scene.render.image_height = *opts.height;
    if (opts.samples_per_pixel) scene.render.samples_per_pixel = *opts.samples_per_pixel;
    if (opts.max_depth) scene.render.max_depth = *opts.max_depth;
    if (opts.seed) scene.render.seed = *opts.seed;
}

int resolve_render_threads(std::optional<int> requested) noexcept {
    const int hardware = static_cast<int>(ThreadPool::default_worker_count()) + 1;

    if (!requested.has_value()) return std::max(hardware - 1, 1);
    if (*requested == -1) return std::max(hardware - 1, 1);
    return *requested;
}

} // namespace pt
