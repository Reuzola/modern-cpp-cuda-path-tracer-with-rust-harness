#pragma once
#include "random.hpp"
#include "vec3.hpp"
#include <algorithm>
#include <array>
#include <cmath>
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
            const double u = p.x() - std::floor(p.x());
            const double v = p.y() - std::floor(p.y());
            const double w = p.z() - std::floor(p.z());

            const int i = static_cast<int>(std::floor(p.x()));
            const int j = static_cast<int>(std::floor(p.y()));
            const int k = static_cast<int>(std::floor(p.z()));

            double c[2][2][2];

            for (int di = 0; di < 2; di++) {
                for (int dj = 0; dj < 2; dj++) {
                    for (int dk = 0; dk < 2; dk++) {
                        c[di][dj][dk] = randfloat[perm_x[(i+di) & 255] ^ perm_y[(j+dj) & 255] ^ perm_z[(k+dk) & 255]];
                    }
                }
            }
            return trilinear_interp(c, u, v, w);
        }
    private:
        static constexpr int point_count{256};
        std::array<double, point_count> randfloat;
        std::array<int, point_count> perm_x, perm_y, perm_z;

        [[nodiscard]] static double trilinear_interp(const double c[2][2][2], double u, double v, double w) {
            double accum{0};

            for (int i = 0; i < 2; i++) {
                for (int j = 0; j < 2; j++) {
                    for (int k = 0; k < 2; k++) {
                        accum += (i*u + (1-i)*(1-u)) * (j*v + (1-j)*(1-v)) * (k*w + (1-k)*(1-w)) * c[i][j][k];
                    }
                }
            }
            return accum;
        }
};