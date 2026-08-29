#include "pt/render/tile.hpp"
#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <type_traits>
#include <vector>

namespace {

using pt::Tile;
using pt::make_tiles;

/// Counts how many tiles claim each pixel. Exactly one is the only right answer:
/// a zero is a black band in the image, a two is two threads writing the same
/// accumulator slot once the passes run in parallel.
[[nodiscard]] std::vector<int> coverage(int width, int height, int tile_size) {
    std::vector<int> counts(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), 0);

    for (const Tile& tile : make_tiles(width, height, tile_size)) {
        for (int y = tile.y0; y < tile.y1; ++y) {
            for (int x = tile.x0; x < tile.x1; ++x) {
                ++counts[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)];
            }
        }
    }
    return counts;
}

void require_exact_cover(int width, int height, int tile_size) {
    const std::vector<int> counts = coverage(width, height, tile_size);

    int wrong = 0;
    for (const int count : counts) {
        if (count != 1) ++wrong;
    }

    INFO("image " << width << "x" << height << " with tile size " << tile_size);
    REQUIRE(wrong == 0);
}

} // namespace

TEST_CASE("tiles cover the image exactly once", "[render][tile]") {
    // A divisible case, an awkward one on both axes, a prime width, and sizes
    // above and below the image.
    require_exact_cover(64, 64, 16);
    require_exact_cover(100, 37, 16);
    require_exact_cover(13, 29, 5);
    require_exact_cover(7, 3, 1);
    require_exact_cover(7, 3, 64);
    require_exact_cover(1, 1, 16);
}

TEST_CASE("the tile count is the ceiling of the division", "[render][tile]") {
    REQUIRE(make_tiles(64, 64, 16).size() == 16);

    // 100/16 is 6.25 and 37/16 is 2.3: seven columns by three rows, with the
    // last of each clipped.
    REQUIRE(make_tiles(100, 37, 16).size() == 21);

    // One tile per pixel, and one tile for the whole image.
    REQUIRE(make_tiles(4, 3, 1).size() == 12);
    REQUIRE(make_tiles(4, 3, 64).size() == 1);
}

TEST_CASE("tile bounds are half-open and clipped to the image", "[render][tile]") {
    const std::vector<Tile> tiles = make_tiles(10, 6, 4);
    REQUIRE(tiles.size() == 6);

    // x1 and y1 are one past the last pixel, like an iterator range - so width()
    // is a subtraction and the loops need no correction.
    REQUIRE(tiles.front().x0 == 0);
    REQUIRE(tiles.front().y0 == 0);
    REQUIRE(tiles.front().x1 == 4);
    REQUIRE(tiles.front().y1 == 4);
    REQUIRE(tiles.front().width() == 4);
    REQUIRE(tiles.front().height() == 4);

    // The last tile is clipped on both axes: 10 is not a multiple of 4 and
    // neither is 6. A remainder tile is smaller, never out of bounds, and never
    // padded - the accumulator has no room for pixels outside the image.
    REQUIRE(tiles.back().x0 == 8);
    REQUIRE(tiles.back().y0 == 4);
    REQUIRE(tiles.back().x1 == 10);
    REQUIRE(tiles.back().y1 == 6);
    REQUIRE(tiles.back().width() == 2);
    REQUIRE(tiles.back().height() == 2);
}

TEST_CASE("tiles come out in row-major order", "[render][tile]") {
    const std::vector<Tile> tiles = make_tiles(10, 6, 4);

    // Left to right, then top to bottom. The order is not load-bearing for
    // correctness - the image is the same whatever order the tiles are visited
    // in, which is exactly what makes parallelising the loop safe - but it does
    // decide the order work is handed out, so it is stated rather than assumed.
    for (std::size_t i = 1; i < tiles.size(); ++i) {
        const Tile& previous = tiles[i - 1];
        const Tile& current = tiles[i];

        const bool same_row_moving_right = current.y0 == previous.y0 && current.x0 > previous.x0;
        const bool next_row_from_the_left = current.y0 > previous.y0 && current.x0 == 0;
        REQUIRE((same_row_moving_right || next_row_from_the_left));
    }
}

TEST_CASE("a tile is a plain aggregate", "[render][tile]") {
    // Trivial and small enough to copy freely: a worker takes a Tile by value,
    // and nothing about the type gets in the way of handing one to a GPU kernel
    // or storing an array of them contiguously.
    STATIC_REQUIRE(std::is_trivially_copyable_v<Tile>);
    STATIC_REQUIRE(std::is_aggregate_v<Tile>);

    constexpr Tile tile{2, 3, 7, 11};
    STATIC_REQUIRE(tile.width() == 5);
    STATIC_REQUIRE(tile.height() == 8);
}
