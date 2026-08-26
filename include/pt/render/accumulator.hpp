#pragma once
#include "pt/math/color.hpp"
#include "pt/math/scalar.hpp"
#include "pt/render/film.hpp"
#include <cassert>
#include <cstddef>
#include <vector>

namespace pt {

class Accumulator final {
public:
    Accumulator(int width, int height)
        : width_(width), height_(height),
          sum_(static_cast<std::size_t>(width) * static_cast<std::size_t>(height)) {
        assert(width > 0 && height > 0);
    }

    void add_sample(int x, int y, const Color& value) noexcept {
        sum_[index(x, y)] += value;
    }

    void end_pass() noexcept { ++sample_count_; }

    [[nodiscard]] Film resolve() const {
        Film film(width_, height_);
        if (sample_count_ == 0) return film;

        const Float scale = 1.0_f / static_cast<Float>(sample_count_);
        for (int y = 0; y < height_; ++y) {
            for (int x = 0; x < width_; ++x) {
                film.set_pixel(x, y, scale * sum_[index(x, y)]);
            }
        }
        return film;
    }

    [[nodiscard]] int width() const noexcept { return width_; }
    [[nodiscard]] int height() const noexcept { return height_; }
    [[nodiscard]] int sample_count() const noexcept { return sample_count_; }

private:
    int width_{};
    int height_{};
    int sample_count_{};
    std::vector<Color> sum_;

    // Indexing: y * width_ + x (Row-major order).
    // Convention: y = 0 is the top scanline, x is the column.
    [[nodiscard]] std::size_t index(int x, int y) const noexcept {
        assert(x >= 0 && x < width_ && y >= 0 && y < height_);
        return (static_cast<std::size_t>(y) * static_cast<std::size_t>(width_)) + static_cast<std::size_t>(x);
    }
};

} // namespace pt
