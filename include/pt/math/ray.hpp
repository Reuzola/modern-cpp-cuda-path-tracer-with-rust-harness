#pragma once
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"

namespace pt {

class Ray {
public:
    constexpr Ray() noexcept = default;
    constexpr Ray(const Point3& origin, const Vec3& direction) noexcept : Ray(origin, direction, 0.0_f) {}
    constexpr Ray(const Point3& origin, const Vec3& direction, Float tm) noexcept : orig_(origin), dir_(direction), tm_(tm) {}

    [[nodiscard]] constexpr const Point3& origin() const noexcept { return orig_; }
    [[nodiscard]] constexpr const Vec3& direction() const noexcept { return dir_; }
    [[nodiscard]] constexpr Float time() const noexcept { return tm_; }

    [[nodiscard]] constexpr Point3 at(Float t) const noexcept {
        return orig_ + t * dir_;
    }

private:
    Point3 orig_;
    Vec3 dir_;
    Float tm_{};
};

// Contract guards: these fail the build if a member below silently loses constexpr.
static_assert(Ray(Point3(1, 2, 3), Vec3(0, 0, 1)).at(2.0_f).z() == 5.0_f);

} // namespace pt
