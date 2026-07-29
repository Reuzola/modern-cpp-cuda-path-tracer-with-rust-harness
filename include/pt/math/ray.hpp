#pragma once
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"

namespace pt {

class Ray {
public:
    Ray() = default;
    Ray(const Point3& origin, const Vec3& direction) : Ray(origin, direction, 0.0_f) {}
    Ray(const Point3& origin, const Vec3& direction, Float tm) : orig_(origin), dir_(direction), tm_(tm) {}

    [[nodiscard]] const Point3& origin() const { return orig_; }
    [[nodiscard]] const Vec3& direction() const { return dir_; }
    [[nodiscard]] Float time() const { return tm_; }

    Point3 at(Float t) const {
        return orig_ + t * dir_;
    }

private:
    Point3 orig_;
    Vec3 dir_;
    Float tm_{};
};

} // namespace pt
