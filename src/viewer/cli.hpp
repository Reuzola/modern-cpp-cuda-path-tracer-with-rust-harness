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
};

[[nodiscard]] std::variant<ViewerOptions, int> parse_viewer_command_line(int argc, char** argv);

void apply_overrides(Scene& scene, const ViewerOptions& opts);

} // namespace pt
