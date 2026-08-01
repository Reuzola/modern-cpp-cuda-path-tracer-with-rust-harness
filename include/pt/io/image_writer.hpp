#pragma once
#include "pt/io/image_format.hpp"
#include <filesystem>
#include <memory>

namespace pt {

class Film;

class ImageWriter {
public:
    ImageWriter() = default;

    ImageWriter(const ImageWriter&) = delete;

    virtual ~ImageWriter() = default;

    ImageWriter& operator=(const ImageWriter&) = delete;

    [[nodiscard]] virtual bool write(const Film& film, const std::filesystem::path& path) const = 0;
};

[[nodiscard]] std::unique_ptr<ImageWriter> make_image_writer(ImageFormat format);

} // namespace pt
