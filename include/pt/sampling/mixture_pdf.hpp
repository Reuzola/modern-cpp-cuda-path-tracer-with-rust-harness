#pragma once
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include "pt/sampling/pdf.hpp"

namespace pt {

class MixturePdf final : public Pdf {
public:
    MixturePdf(const Pdf& p1, const Pdf& p2) : p1(p1), p2(p2) {}

    [[nodiscard]] Float value(const Vec3& direction) const override;

    [[nodiscard]] Vec3 generate() const override;

private:
    const Pdf& p1;
    const Pdf& p2;
    static constexpr Float weight = 0.5_f;
};

} // namespace pt
