#pragma once
#include "pt/math/color.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"

namespace pt {

class texture {
public:
    virtual ~texture();

    [[nodiscard]] virtual color value(Float u, Float v, const point3& p) const = 0;
};

} // namespace pt
