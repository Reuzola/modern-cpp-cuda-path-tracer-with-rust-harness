#pragma once
#include "vec3.hpp"
#include <array>
#include <cmath>

class onb {
    public:
        explicit onb(const vec3& n) {
            axis[2] = unit_vector(n);
            const vec3 a = (std::fabs(axis[2].x()) > 0.9) ? vec3(0, 1, 0) : vec3(1, 0, 0);
            axis[1] = unit_vector(cross(axis[2], a));
            axis[0] = cross(axis[2], axis[1]);
        }

        [[nodiscard]] const vec3& u() const { return axis[0]; }
        [[nodiscard]] const vec3& v() const { return axis[1]; }
        [[nodiscard]] const vec3& w() const { return axis[2]; }

        [[nodiscard]] vec3 transform(const vec3& local) const {
            return local[0] * axis[0] + local[1] * axis[1] + local[2] * axis[2];
        }
    private:
        std::array<vec3, 3> axis; // axis[2] -> normal
};