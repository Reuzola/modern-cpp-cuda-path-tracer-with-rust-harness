#pragma once
#include "pt/io/image_format.hpp"
#include <cstdint>
#include <filesystem>
#include <optional>
#include <variant>

namespace pt {

class Scene;

struct CliOptions { // CLI options with default values
    std::filesystem::path scene;
    std::filesystem::path output{"out/image.png"};
    ImageFormat format{ImageFormat::png};
    std::optional<int> width;
    std::optional<int> height;
    std::optional<int> samples_per_pixel;
    std::optional<int> max_depth;
    std::optional<std::uint64_t> seed;
};

[[nodiscard]] std::variant<CliOptions, int> parse_command_line(int argc, char** argv);

void apply_overrides(Scene& scene, const CliOptions& opts);

[[nodiscard]] int resolve_image_height(const Scene& scene, const CliOptions& opts);

} // namespace pt
