#include "viewer/screenshot.hpp"
#include "pt/io/image_format.hpp"
#include "pt/io/image_writer.hpp"
#include "pt/util/log.hpp"
#include <chrono>
#include <filesystem>
#include <format>
#include <memory>
#include <string>
#include <string_view>
#include <system_error>

namespace pt {

namespace {

[[nodiscard]] std::string_view extension_for(ImageFormat format) noexcept {
    // No 'default': a new enumerator must break this switch.
    switch (format) {
    case ImageFormat::ppm: return ".ppm";
    case ImageFormat::png: return ".png";
    case ImageFormat::exr: return ".exr";
    }
    return ".png";
}

[[nodiscard]] std::string timestamped_name(ImageFormat format) {
    const auto now = std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now());
    return std::format("screenshot_{:%Y%m%d_%H%M%S}{}", now, extension_for(format));
}

} // namespace

void save_screenshot(const Film& film, ImageFormat format) {
    // Fixed location: the repo's .gitignore already excludes out/.
    static constexpr std::string_view output_dir = "out/screenshots";

    std::error_code ec;
    std::filesystem::create_directories(output_dir, ec);
    if (ec) {
        log_error("Cannot create directory: {}", ec.message());
        return;
    }

    const std::filesystem::path path = std::filesystem::path(output_dir) / timestamped_name(format);
    const std::unique_ptr<ImageWriter> writer = make_image_writer(format);
    if (!writer->write(film, path)) {
        log_error("Could not write screenshot '{}'", path.string());
        return;
    }

    log_info("Saved screenshot '{}'", path.string());
}

} // namespace pt
