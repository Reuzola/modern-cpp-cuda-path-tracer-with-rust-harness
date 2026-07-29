#pragma once
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include <array>
#include <cmath>

namespace pt {

class Onb {
public:
    explicit Onb(const Vec3& n) {
        axis_[2] = unit_vector(n);
        const Vec3 a = (std::fabs(axis_[2].x()) > 0.9_f) ? Vec3(0, 1, 0) : Vec3(1, 0, 0);
        axis_[1] = unit_vector(cross(axis_[2], a));
        axis_[0] = cross(axis_[2], axis_[1]);
    }

    [[nodiscard]] const Vec3& u() const { return axis_[0]; }
    [[nodiscard]] const Vec3& v() const { return axis_[1]; }
    [[nodiscard]] const Vec3& w() const { return axis_[2]; }

    [[nodiscard]] Vec3 transform(const Vec3& local) const {
        return local[0] * axis_[0] + local[1] * axis_[1] + local[2] * axis_[2];
    }

private:
    std::array<Vec3, 3> axis_; // axis[2] -> normal
};

} // namespace pt
