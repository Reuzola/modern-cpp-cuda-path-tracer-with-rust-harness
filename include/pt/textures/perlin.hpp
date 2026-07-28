#pragma once
#include "pt/math/vec3.hpp"
#include <array>

namespace pt {

class perlin {
public:
    perlin();

    [[nodiscard]] double noise(const point3& p) const;

    [[nodiscard]] double turb(const point3& p, int depth) const;

private:
    static constexpr int point_count{256};
    std::array<vec3, point_count> randvec;
    std::array<int, point_count> perm_x, perm_y, perm_z;

    [[nodiscard]] static double perlin_interp(const vec3 c[2][2][2], double u, double v, double w);
};

} // namespace pt
