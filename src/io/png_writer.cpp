#include "pt/io/png_writer.hpp"
#include "pt/io/color.hpp"
#include "pt/render/film.hpp"
#include "stb_image_write.h"
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace pt {

bool PngWriter::write(const Film& film, const std::filesystem::path& path) const {
    static constexpr int channels = 3;
    const int width = film.width();
    const int height = film.height();

    const std::size_t byte_count =
        static_cast<std::size_t>(width) *
        static_cast<std::size_t>(height) *
        static_cast<std::size_t>(channels);

    std::vector<std::uint8_t> buffer(byte_count);
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            const auto [r, g, b] = to_ldr_bytes(film.pixel(x, y));

            const std::size_t px = static_cast<std::size_t>(x);
            const std::size_t py = static_cast<std::size_t>(y);
            const std::size_t index =
                ((py * static_cast<std::size_t>(width)) + px) * static_cast<std::size_t>(channels);

            buffer[index] = r;
            buffer[index + 1] = g;
            buffer[index + 2] = b;
        }
    }
    // Extends lifetime to safely pass c_str() to C APIs.
    const std::string filename = path.string();

    const int result = stbi_write_png(filename.c_str(), width, height, channels, buffer.data(), 0);
    return result != 0;
}

} // namespace pt
