#include "pt/render/tile.hpp"
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <vector>

namespace pt {

std::vector<Tile> make_tiles(int image_width, int image_height, int tile_size) {
    assert(image_width > 0 && image_height > 0 && tile_size > 0);

    const int tiles_x = (image_width + tile_size - 1) / tile_size;
    const int tiles_y = (image_height + tile_size - 1) / tile_size;

    std::vector<Tile> tiles;
    tiles.reserve(static_cast<std::size_t>(tiles_x) * static_cast<std::size_t>(tiles_y));

    for (int y0 = 0; y0 < image_height; y0 += tile_size) {
        const int y1 = std::min(y0 + tile_size, image_height);
        for (int x0 = 0; x0 < image_width; x0 += tile_size) {
            const int x1 = std::min(x0 + tile_size, image_width);
            tiles.push_back(Tile{x0, y0, x1, y1});
        }
    }
    return tiles;
}

} // namespace pt
