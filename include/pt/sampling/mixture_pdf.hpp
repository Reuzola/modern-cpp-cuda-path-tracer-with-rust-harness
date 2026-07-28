#pragma once
#include "pt/math/vec3.hpp"
#include "pt/sampling/pdf.hpp"

namespace pt {

class mixture_pdf final : public pdf {
public:
    mixture_pdf(const pdf& p1, const pdf& p2) : p1(p1), p2(p2) {}

    [[nodiscard]] double value(const vec3& direction) const override;

    [[nodiscard]] vec3 generate() const override;

private:
    const pdf& p1;
    const pdf& p2;
    static constexpr double weight = 0.5;
};

} // namespace pt
