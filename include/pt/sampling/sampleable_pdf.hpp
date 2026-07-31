#pragma once
#include "pt/core/sampleable.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include "pt/sampling/pdf.hpp"

namespace pt {

class Sampleable;

class Sampler;

class SampleablePdf final : public Pdf {
public:
    SampleablePdf(const Sampleable& target, const Point3& origin) : target_(target), origin_(origin) {}

    [[nodiscard]] Float value(const Vec3& direction) const override;

    [[nodiscard]] Vec3 generate(Sampler& sampler) const override;

private:
    const Sampleable& target_;
    Point3 origin_;
};

} // namespace pt
