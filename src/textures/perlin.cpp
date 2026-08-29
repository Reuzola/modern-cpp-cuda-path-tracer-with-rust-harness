#include "pt/textures/perlin.hpp"
#include "pt/math/sampler.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <span>

namespace pt {

namespace {

void shuffle_permutation(std::span<int> perm, Sampler& sampler) {
    for (int i = static_cast<int>(perm.size()) - 1; i > 0; i--) {
        const int j = static_cast<int>(sampler.next_below(static_cast<std::uint32_t>(i) + 1U));
        std::swap(perm[static_cast<std::size_t>(i)], perm[static_cast<std::size_t>(j)]);
    }
}

[[nodiscard]] constexpr std::size_t corner_index(int di, int dj, int dk) noexcept {
    return (static_cast<std::size_t>(di) * 4) + (static_cast<std::size_t>(dj) * 2) + static_cast<std::size_t>(dk);
}

} // namespace

Perlin::Perlin(Sampler& sampler) {
    std::ranges::generate(randvec_, [&sampler] { return unit_vector(Vec3::random(-1, 1, sampler)); });

    std::iota(perm_x_.begin(), perm_x_.end(), 0);
    std::iota(perm_y_.begin(), perm_y_.end(), 0);
    std::iota(perm_z_.begin(), perm_z_.end(), 0);

    shuffle_permutation(perm_x_, sampler);
    shuffle_permutation(perm_y_, sampler);
    shuffle_permutation(perm_z_, sampler);
}

Float Perlin::noise(const Point3& p) const {
    const Float u = p.x() - std::floor(p.x());
    const Float v = p.y() - std::floor(p.y());
    const Float w = p.z() - std::floor(p.z());

    const int i = static_cast<int>(std::floor(p.x()));
    const int j = static_cast<int>(std::floor(p.y()));
    const int k = static_cast<int>(std::floor(p.z()));

    GradientCorners c;

    for (int di = 0; di < 2; di++) {
        for (int dj = 0; dj < 2; dj++) {
            for (int dk = 0; dk < 2; dk++) {
                const std::size_t index = static_cast<std::size_t>(perm_x_[(i + di) & 255] ^ perm_y_[(j + dj) & 255] ^ perm_z_[(k + dk) & 255]);
                c[corner_index(di, dj, dk)] = randvec_[index];
            }
        }
    }
    return perlin_interp(c, u, v, w);
}

Float Perlin::turb(const Point3& p, int depth) const {
    Float accum{0};
    Point3 temp_p = p;
    Float weight{1.0_f};

    for (int i = 0; i < depth; i++) {
        accum += weight * noise(temp_p);
        weight *= 0.5_f;
        temp_p *= 2;
    }
    return std::fabs(accum);
}

Float Perlin::perlin_interp(const GradientCorners& c, Float u, Float v, Float w) {
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
                const Vec3 weight_v(u - fi, v - fj, w - fk);

                accum += (fi * uu + (1 - fi) * (1 - uu)) *
                         (fj * vv + (1 - fj) * (1 - vv)) *
                         (fk * ww + (1 - fk) * (1 - ww)) *
                         dot(c[corner_index(i, j, k)], weight_v);
            }
        }
    }
    return accum;
}

} // namespace pt
