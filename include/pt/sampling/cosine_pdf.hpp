#pragma once
#include "pt/math/onb.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include "pt/sampling/pdf.hpp"

namespace pt {

class Sampler;

class CosinePdf final : public Pdf {
public:
    explicit CosinePdf(const Vec3& normal) : uvw_(normal) {}

    [[nodiscard]] Float value(const Vec3& direction) const override;

    [[nodiscard]] Vec3 generate(Sampler& sampler) const override;

private:
    Onb uvw_;
};

} // namespace pt
