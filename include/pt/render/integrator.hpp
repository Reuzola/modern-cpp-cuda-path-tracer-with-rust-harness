#pragma once
#include "pt/math/color.hpp"
#include "pt/math/ray.hpp"

namespace pt {

class Sampler;

class Integrator {
public:
    // By value: an integrator walks a path by replacing the ray, segment by segment.
    [[nodiscard]] virtual Color radiance(Ray ray, Sampler& sampler) const = 0;

protected:
    ~Integrator() = default;
};

} // namespace pt
