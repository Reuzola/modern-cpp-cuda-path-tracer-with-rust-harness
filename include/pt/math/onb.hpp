#pragma once
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include <array>
#include <cmath>

namespace pt {

class Onb {
public:
    explicit Onb(const Vec3& n) {
        axis[2] = unit_vector(n);
        const Vec3 a = (std::fabs(axis[2].x()) > 0.9_f) ? Vec3(0, 1, 0) : Vec3(1, 0, 0);
        axis[1] = unit_vector(cross(axis[2], a));
        axis[0] = cross(axis[2], axis[1]);
    }

    [[nodiscard]] const Vec3& u() const { return axis[0]; }
    [[nodiscard]] const Vec3& v() const { return axis[1]; }
    [[nodiscard]] const Vec3& w() const { return axis[2]; }

    [[nodiscard]] Vec3 transform(const Vec3& local) const {
        return local[0] * axis[0] + local[1] * axis[1] + local[2] * axis[2];
    }

private:
    std::array<Vec3, 3> axis; // axis[2] -> normal
};

} // namespace pt
