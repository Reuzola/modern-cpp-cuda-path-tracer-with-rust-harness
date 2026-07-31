#pragma once
#include "pt/math/color.hpp"
#include "pt/math/ray.hpp"

namespace pt {

class Sampler;

class Integrator {
public:
    [[nodiscard]] virtual Color radiance(const Ray& r, Sampler& sampler) const = 0;

protected:
    ~Integrator() = default;
};

} // namespace pt
