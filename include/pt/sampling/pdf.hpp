#pragma once
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"

namespace pt {

class Sampler;

class Pdf {
public:
    [[nodiscard]] virtual Float value(const Vec3& direction) const = 0;

    [[nodiscard]] virtual Vec3 generate(Sampler& sampler) const = 0;

protected:
    ~Pdf() = default;
};

} // namespace pt
