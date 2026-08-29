#pragma once
#include "pt/io/image_writer.hpp"
#include <filesystem>

namespace pt {

class ExrWriter final : public ImageWriter {
public:
    [[nodiscard]] bool write(const Film& film, const std::filesystem::path& path) const override;
};

} // namespace pt
