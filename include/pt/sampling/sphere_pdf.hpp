#pragma once
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include "pt/sampling/pdf.hpp"

namespace pt {

class SpherePdf final : public Pdf {
public:
    [[nodiscard]] Float value(const Vec3& direction) const override;

    [[nodiscard]] Vec3 generate() const override;
};

} // namespace pt
