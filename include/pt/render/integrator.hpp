#pragma once
#include "pt/math/color.hpp"
#include "pt/math/ray.hpp"

namespace pt {

class Integrator {
public:
    [[nodiscard]] virtual Color radiance(const Ray& r) const = 0;

protected:
    ~Integrator() = default;
};

} // namespace pt
