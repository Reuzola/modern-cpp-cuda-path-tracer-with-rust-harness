#pragma once
#include "pt/math/onb.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include "pt/sampling/pdf.hpp"

namespace pt {

class CosinePdf final : public Pdf {
public:
    explicit CosinePdf(const Vec3& normal) : uvw(normal) {}

    [[nodiscard]] Float value(const Vec3& direction) const override;

    [[nodiscard]] Vec3 generate() const override;

private:
    Onb uvw;
};

} // namespace pt
