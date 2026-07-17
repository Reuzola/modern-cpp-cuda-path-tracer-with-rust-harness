#pragma once
#include "vec3.hpp"

class texture {
    public:
        virtual ~texture() = default;

        [[nodiscard]] virtual color value(double u, double v, const point3& p) const = 0;
};