#pragma once
#include <vector>

namespace pt {

struct Tile {
    int x0{};
    int y0{};
    int x1{};
    int y1{};

    [[nodiscard]] constexpr int width() const noexcept { return x1 - x0; }

    [[nodiscard]] constexpr int height() const noexcept { return y1 - y0; }
};

[[nodiscard]] std::vector<Tile> make_tiles(int image_width, int image_height, int tile_size);

} // namespace pt
