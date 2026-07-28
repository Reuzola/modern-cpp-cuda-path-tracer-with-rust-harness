#pragma once
#include "pt/math/vec3.hpp"

namespace pt {

class texture {
public:
    virtual ~texture();

    [[nodiscard]] virtual color value(double u, double v, const point3& p) const = 0;
};

} // namespace pt
