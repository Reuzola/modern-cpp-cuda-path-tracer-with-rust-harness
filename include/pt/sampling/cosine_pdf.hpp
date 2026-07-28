#pragma once
#include "pt/math/onb.hpp"
#include "pt/math/vec3.hpp"
#include "pt/sampling/pdf.hpp"

namespace pt {

class cosine_pdf final : public pdf {
public:
    explicit cosine_pdf(const vec3& normal) : uvw(normal) {}

    [[nodiscard]] double value(const vec3& direction) const override;

    [[nodiscard]] vec3 generate() const override;

private:
    onb uvw;
};

} // namespace pt
