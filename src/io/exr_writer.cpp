#include "pt/io/exr_writer.hpp"
#include "pt/math/color.hpp"
#include "pt/render/film.hpp"
#include <OpenEXR/ImfChannelList.h>
#include <OpenEXR/ImfCompression.h>
#include <OpenEXR/ImfFrameBuffer.h>
#include <OpenEXR/ImfHeader.h>
#include <OpenEXR/ImfOutputFile.h>
#include <OpenEXR/ImfPixelType.h>
#include <cstddef>
#include <exception>
#include <filesystem>
#include <string>
#include <vector>

namespace pt {

bool ExrWriter::write(const Film& film, const std::filesystem::path& path) const {
    try {
        static constexpr int channels = 3;
        const int width = film.width();
        const int height = film.height();

        const std::size_t float_count =
            static_cast<std::size_t>(width) *
            static_cast<std::size_t>(height) *
            static_cast<std::size_t>(channels);

        std::vector<float> buffer(float_count);
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                const Color c = film.pixel(x, y);
                const float r = static_cast<float>(c.r());
                const float g = static_cast<float>(c.g());
                const float b = static_cast<float>(c.b());

                const std::size_t px = static_cast<std::size_t>(x);
                const std::size_t py = static_cast<std::size_t>(y);
                const std::size_t index =
                    ((py * static_cast<std::size_t>(width)) + px) * static_cast<std::size_t>(channels);

                buffer[index] = r;
                buffer[index + 1] = g;
                buffer[index + 2] = b;
            }
        }
        Imf::Header header(width, height);

        header.channels().insert("R", Imf::Channel(Imf::FLOAT));
        header.channels().insert("G", Imf::Channel(Imf::FLOAT));
        header.channels().insert("B", Imf::Channel(Imf::FLOAT));

        header.compression() = Imf::ZIP_COMPRESSION;

        // Extends lifetime to safely pass c_str() to C APIs.
        const std::string filename = path.string();
        Imf::OutputFile file(filename.c_str(), header);

        char* const base = reinterpret_cast<char*>(buffer.data());

        const std::size_t x_stride = static_cast<std::size_t>(channels) * sizeof(float);
        const std::size_t y_stride = x_stride * static_cast<std::size_t>(width);

        Imf::FrameBuffer fb;
        fb.insert("R", Imf::Slice(Imf::FLOAT, base, x_stride, y_stride));
        fb.insert("G", Imf::Slice(Imf::FLOAT, base + sizeof(float), x_stride, y_stride));
        fb.insert("B", Imf::Slice(Imf::FLOAT, base + (2 * sizeof(float)), x_stride, y_stride));

        file.setFrameBuffer(fb);
        file.writePixels(height);

        return true;
    } catch (const std::exception&) {
        return false;
    }
}

} // namespace pt
