#pragma once
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"

namespace pt {

class Ray {
public:
    Ray() = default;
    Ray(const Point3& origin, const Vec3& direction) : Ray(origin, direction, 0.0_f) {}
    Ray(const Point3& origin, const Vec3& direction, Float tm) : orig(origin), dir(direction), tm(tm) {}

    [[nodiscard]] const Point3& origin() const { return orig; }
    [[nodiscard]] const Vec3& direction() const { return dir; }
    [[nodiscard]] Float time() const { return tm; }

    Point3 at(Float t) const {
        return orig + t * dir;
    }

private:
    Point3 orig;
    Vec3 dir;
    Float tm{};
};

} // namespace pt
