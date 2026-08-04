#include "cli.hpp"
#include "pt/io/image_format.hpp"
#include "pt/scene/scene.hpp"
#include "pt/util/log.hpp"
#include <CLI/CLI.hpp>
#include <limits>
#include <map>
#include <string>
#include <variant>

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
    app.set_version_flag("--version", "0.1.0");

    CliOptions opts;

    app.add_option("scene", opts.scene, "scene file to render")->required();
    app.add_option("-o,--output", opts.output, "Output image path")->capture_default_str();
    app.add_option("--format", opts.format, "Output image format")
        ->transform(CLI::CheckedTransformer(format_map, CLI::ignore_case))
        ->option_text("FORMAT:{ppm, png, exr}")
        ->capture_default_str();
    app.add_option("--log-level", opts.log_level, "Logging verbosity")
        ->transform(CLI::CheckedTransformer(log_level_map, CLI::ignore_case))
        ->option_text("LEVEL:{info, warning, error, off}")
        ->capture_default_str();

    CLI::Option* width_opt = app.add_option("--width", opts.width, "image width to render")
                                 ->check(positive_int);
    CLI::Option* height_opt = app.add_option("--height", opts.height, "image height to render")
                                  ->check(positive_int);
    width_opt->needs(height_opt);
    height_opt->needs(width_opt);

    app.add_option("--spp", opts.samples_per_pixel, "samples per pixel")
        ->check(positive_int);
    app.add_option("--max-depth", opts.max_depth, "maximum ray bounce depth")
        ->check(positive_int);
    app.add_option("--seed", opts.seed, "random seed");

    try {
        app.parse(argc, argv);
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

} // namespace pt
