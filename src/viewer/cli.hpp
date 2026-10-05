#pragma once
#include "pt/util/log.hpp"
#include <cstdint>
#include <filesystem>
#include <optional>
#include <variant>

namespace pt {

class Scene;

struct ViewerOptions {
    std::filesystem::path scene;
    LogLevel log_level{LogLevel::info};
    std::optional<int> width;
    std::optional<int> height;
    std::optional<int> samples_per_pixel;
    std::optional<int> max_depth;
    std::optional<std::uint64_t> seed;
    std::optional<int> threads;
    std::optional<int> measure_frames; // Set: scripted measurement run, record on stdout, then exit.
    std::optional<float> ui_scale;     // Overrides the platform's content scale (XWayland reports none).
};

[[nodiscard]] std::variant<ViewerOptions, int> parse_viewer_command_line(int argc, char** argv);

void apply_overrides(Scene& scene, const ViewerOptions& opts);

// Same values as the offline driver's --threads, kept separate on purpose: the
// viewer's thread also feeds the UI, so its default is a policy of its own.
[[nodiscard]] int resolve_render_threads(std::optional<int> requested) noexcept;

} // namespace pt
