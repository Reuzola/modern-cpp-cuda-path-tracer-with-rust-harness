#pragma once
#include "pt/math/vec3.hpp"
#include "pt/sampling/pdf.hpp"

namespace pt {

class sphere_pdf final : public pdf {
public:
    [[nodiscard]] double value(const vec3& direction) const override;

    [[nodiscard]] vec3 generate() const override;
};

} // namespace pt
