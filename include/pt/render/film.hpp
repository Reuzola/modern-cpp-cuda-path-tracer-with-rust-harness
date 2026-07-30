#pragma once
#include "pt/math/color.hpp"
#include <cassert>
#include <cstddef>
#include <vector>

namespace pt {

class Film {
public:
    explicit Film(int width, int height) : width_(width), height_(height), pixels_(buffer_size(width, height)) {}

    [[nodiscard]] int width() const noexcept { return width_; }

    [[nodiscard]] int height() const noexcept { return height_; }

    [[nodiscard]] Color pixel(int x, int y) const noexcept {
        return pixels_[index(x, y)];
    }

    void set_pixel(int x, int y, const Color& value) noexcept {
        pixels_[index(x, y)] = value;
    }

private:
    int width_{};
    int height_{};
    std::vector<Color> pixels_;

    // Indexing: y * width_ + x (Row-major order).
    // Convention: y = 0 is the top scanline, x is the column.
    [[nodiscard]] std::size_t index(int x, int y) const noexcept {
        assert(x >= 0 && x < width_ && y >= 0 && y < height_);
        return (static_cast<std::size_t>(y) * static_cast<std::size_t>(width_)) + static_cast<std::size_t>(x);
    }

    [[nodiscard]] static std::size_t buffer_size(int width, int height) noexcept {
        assert(width > 0 && height > 0);
        return static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    }
};

} // namespace pt
