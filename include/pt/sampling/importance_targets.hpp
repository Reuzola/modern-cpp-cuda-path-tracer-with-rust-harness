#pragma once
#include "pt/core/sampleable.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include <vector>

namespace pt {

class Sampler;

class ImportanceTargets final : public Sampleable {
public:
    void add(const Sampleable* target);

    [[nodiscard]] bool empty() const noexcept { return targets_.empty(); }

    [[nodiscard]] Float pdf_direction(const Point3& origin, const Vec3& direction) const override;

    [[nodiscard]] Vec3 sample_direction(const Point3& origin, Sampler& sampler) const override;

private:
    std::vector<const Sampleable*> targets_;
};

} // namespace pt
