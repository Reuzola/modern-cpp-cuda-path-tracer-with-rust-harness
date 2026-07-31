#pragma once
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"

namespace pt {

class Sampler;

class Sampleable {
public:
    [[nodiscard]] virtual Float pdf_direction(const Point3& origin, const Vec3& direction) const = 0;

    [[nodiscard]] virtual Vec3 sample_direction(const Point3& origin, Sampler& sampler) const = 0;

protected:
    ~Sampleable() = default;
};

} // namespace pt
