#pragma once
#include "random.hpp"
#include <algorithm>
#include <array>
#include <numeric>

class perlin {
    public:
        perlin() {
            std::ranges::generate(randfloat, [] { return random_double(); });

            std::iota(perm_x.begin(), perm_x.end(), 0);
            std::iota(perm_y.begin(), perm_y.end(), 0);
            std::iota(perm_z.begin(), perm_z.end(), 0);

            std::ranges::shuffle(perm_x, rng());
            std::ranges::shuffle(perm_y, rng());
            std::ranges::shuffle(perm_z, rng());
        }
    private:
        static constexpr int point_count{256};
        std::array<double, point_count> randfloat;
        std::array<int, point_count> perm_x, perm_y, perm_z;
};