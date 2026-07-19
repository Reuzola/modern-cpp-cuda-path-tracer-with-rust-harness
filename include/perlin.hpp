#pragma once
#include "random.hpp"
#include "vec3.hpp"
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

        [[nodiscard]] double noise(const point3& p) const {
            const int i = static_cast<int>(4 * p.x()) & 255;
            const int j = static_cast<int>(4 * p.y()) & 255;
            const int k = static_cast<int>(4 * p.z()) & 255;

            return randfloat[perm_x[i] ^ perm_y[j] ^ perm_z[k]];
        }
    private:
        static constexpr int point_count{256};
        std::array<double, point_count> randfloat;
        std::array<int, point_count> perm_x, perm_y, perm_z;
};