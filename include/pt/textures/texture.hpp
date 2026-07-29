#pragma once
#include "pt/math/color.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"

namespace pt {

class Texture {
public:
    virtual ~Texture();

    [[nodiscard]] virtual Color value(Float u, Float v, const Point3& p) const = 0;
};

} // namespace pt
