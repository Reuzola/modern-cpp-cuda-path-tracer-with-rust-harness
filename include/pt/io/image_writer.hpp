#pragma once
#include <filesystem>

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

} // namespace pt
