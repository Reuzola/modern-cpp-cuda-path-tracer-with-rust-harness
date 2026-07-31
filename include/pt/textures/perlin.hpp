#pragma once
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include <array>

namespace pt {

class Sampler;

class Perlin {
public:
    explicit Perlin(Sampler& sampler);

    [[nodiscard]] Float noise(const Point3& p) const;

    [[nodiscard]] Float turb(const Point3& p, int depth) const;

private:
    static constexpr int point_count{256};
    std::array<Vec3, point_count> randvec_;
    std::array<int, point_count> perm_x_, perm_y_, perm_z_;

    [[nodiscard]] static Float perlin_interp(const Vec3 c[2][2][2], Float u, Float v, Float w);
};

} // namespace pt
