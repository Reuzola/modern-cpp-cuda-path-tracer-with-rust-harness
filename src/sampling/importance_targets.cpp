#include "pt/sampling/importance_targets.hpp"
#include "pt/core/sampleable.hpp"
#include "pt/math/random.hpp"
#include "pt/math/sampler.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include <cassert>
#include <cstdint>

namespace pt {

void ImportanceTargets::add(const Sampleable* target) {
    assert(target != nullptr);

    targets_.push_back(target);
}

Float ImportanceTargets::pdf_direction(const Point3& origin, const Vec3& direction) const {
    if (targets_.empty()) return 0.0_f;

    const Float weight = 1.0_f / static_cast<Float>(targets_.size());
    Float sum{0.0_f};

    for (const Sampleable* target : targets_) {
        sum += weight * target->pdf_direction(origin, direction);
    }
    return sum;
}

Vec3 ImportanceTargets::sample_direction(const Point3& origin, Sampler& sampler) const {
    if (targets_.empty()) return Vec3(0, 0, 0);

    const auto count = static_cast<std::uint32_t>(targets_.size());
    const std::uint32_t index = sampler.next_below(count);
    return targets_[index]->sample_direction(origin, sampler);
}

} // namespace pt
