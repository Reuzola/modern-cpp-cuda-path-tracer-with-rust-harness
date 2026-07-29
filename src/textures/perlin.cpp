#include "pt/textures/perlin.hpp"
#include "pt/math/random.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numeric>

namespace pt {

perlin::perlin() {
    std::ranges::generate(randvec, [] { return unit_vector(vec3::random(-1, 1)); });

    std::iota(perm_x.begin(), perm_x.end(), 0);
    std::iota(perm_y.begin(), perm_y.end(), 0);
    std::iota(perm_z.begin(), perm_z.end(), 0);

    std::ranges::shuffle(perm_x, rng());
    std::ranges::shuffle(perm_y, rng());
    std::ranges::shuffle(perm_z, rng());
}

Float perlin::noise(const point3& p) const {
    const Float u = p.x() - std::floor(p.x());
    const Float v = p.y() - std::floor(p.y());
    const Float w = p.z() - std::floor(p.z());

    const int i = static_cast<int>(std::floor(p.x()));
    const int j = static_cast<int>(std::floor(p.y()));
    const int k = static_cast<int>(std::floor(p.z()));

    vec3 c[2][2][2];

    for (int di = 0; di < 2; di++) {
        for (int dj = 0; dj < 2; dj++) {
            for (int dk = 0; dk < 2; dk++) {
                const std::size_t index = static_cast<std::size_t>(perm_x[(i + di) & 255] ^ perm_y[(j + dj) & 255] ^ perm_z[(k + dk) & 255]);
                c[di][dj][dk] = randvec[index];
            }
        }
    }
    return perlin_interp(c, u, v, w);
}

Float perlin::turb(const point3& p, int depth) const {
    Float accum{0};
    point3 temp_p = p;
    Float weight{1.0_f};

    for (int i = 0; i < depth; i++) {
        accum += weight * noise(temp_p);
        weight *= 0.5_f;
        temp_p *= 2;
    }
    return std::fabs(accum);
}

Float perlin::perlin_interp(const vec3 c[2][2][2], Float u, Float v, Float w) {
    const Float uu = u * u * (3 - 2 * u); // u = 3u² − 2u³
    const Float vv = v * v * (3 - 2 * v); // v = 3v² − 2v³
    const Float ww = w * w * (3 - 2 * w); // w = 3w² − 2w³

    Float accum{0};
    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 2; j++) {
            for (int k = 0; k < 2; k++) {
                const Float fi = static_cast<Float>(i);
                const Float fj = static_cast<Float>(j);
                const Float fk = static_cast<Float>(k);
                const vec3 weight_v(u - fi, v - fj, w - fk);

                accum += (fi * uu + (1 - fi) * (1 - uu)) * (fj * vv + (1 - fj) * (1 - vv)) * (fk * ww + (1 - fk) * (1 - ww)) * dot(c[i][j][k], weight_v);
            }
        }
    }
    return accum;
}

} // namespace pt
