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
            std::ranges::generate(randvec, [] { return unit_vector(vec3::random(-1, 1)); });

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

            vec3 c[2][2][2];

            for (int di = 0; di < 2; di++) {
                for (int dj = 0; dj < 2; dj++) {
                    for (int dk = 0; dk < 2; dk++) {
                        c[di][dj][dk] = randvec[perm_x[(i+di) & 255] ^ perm_y[(j+dj) & 255] ^ perm_z[(k+dk) & 255]];
                    }
                }
            }
            return perlin_interp(c, u, v, w);
        }

        [[nodiscard]] double turb(const point3& p, int depth) const {
            double accum{0};
            point3 temp_p = p;
            double weight{1.0};

            for (int i = 0; i < depth; i++) {
                accum += weight * noise(temp_p);
                weight *= 0.5;
                temp_p *= 2;
            }
            return std::fabs(accum);
        }
    private:
        static constexpr int point_count{256};
        std::array<vec3, point_count> randvec;
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

        [[nodiscard]] static double perlin_interp(const vec3 c[2][2][2], double u, double v, double w) {
            const double uu = u*u*(3 - 2*u); // u = 3u² − 2u³
            const double vv = v*v*(3 - 2*v); // v = 3v² − 2v³
            const double ww = w*w*(3 - 2*w); // w = 3w² − 2w³
            
            double accum{0};
            for (int i = 0; i < 2; i++) {
                for (int j = 0; j < 2; j++) {
                    for (int k = 0; k < 2; k++) {
                        const vec3 weight_v(u - i, v - j, w - k);
                        accum += (i*uu + (1-i)*(1-uu)) * (j*vv + (1-j)*(1-vv)) * (k*ww + (1-k)*(1-ww)) * dot(c[i][j][k], weight_v);
                    }
                }
            }
            return accum;
        }
};