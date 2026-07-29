#pragma once
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"

namespace pt {

class ray {
public:
    ray() = default;
    ray(const point3& origin, const vec3& direction) : ray(origin, direction, 0.0_f) {}
    ray(const point3& origin, const vec3& direction, Float tm) : orig(origin), dir(direction), tm(tm) {}

    [[nodiscard]] const point3& origin() const { return orig; }
    [[nodiscard]] const vec3& direction() const { return dir; }
    [[nodiscard]] Float time() const { return tm; }

    point3 at(Float t) const {
        return orig + t * dir;
    }

private:
    point3 orig;
    vec3 dir;
    Float tm{};
};

} // namespace pt
