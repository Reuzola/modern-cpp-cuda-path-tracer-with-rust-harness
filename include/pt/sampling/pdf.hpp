#pragma once
#include "pt/math/vec3.hpp"

namespace pt {

class pdf {
public:
    [[nodiscard]] virtual double value(const vec3& direction) const = 0;

    [[nodiscard]] virtual vec3 generate() const = 0;

protected:
    ~pdf() = default;
};

} // namespace pt
