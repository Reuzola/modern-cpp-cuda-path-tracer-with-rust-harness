#include "pt/io/ppm_writer.hpp"
#include "pt/io/color.hpp"
#include "pt/render/film.hpp"
#include <filesystem>
#include <format>
#include <fstream>
#include <ostream>

namespace pt {

bool PpmWriter::write(const Film& film, const std::filesystem::path& path) const {
    std::ofstream out(path);
    if (!out) return false;

    out << std::format("P3\n{} {}\n255\n", film.width(), film.height());

    for (int j = 0; j < film.height(); j++) {
        for (int i = 0; i < film.width(); i++) {
            const auto [r, g, b] = to_ldr_bytes(film.pixel(i, j));
            out << static_cast<int>(r) << ' ' << static_cast<int>(g) << ' ' << static_cast<int>(b) << '\n';
        }
    }
    return static_cast<bool>(out);
}

} // namespace pt
