#include "pt/sampling/importance_targets.hpp"
#include "pt/core/sampleable.hpp"
#include "pt/math/random.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include <cassert>
#include <cstddef>

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

Vec3 ImportanceTargets::sample_direction(const Point3& origin) const {
    if (targets_.empty()) return Vec3(0, 0, 0);

    const int count = static_cast<int>(targets_.size());
    return targets_[static_cast<std::size_t>(random_int(0, count - 1))]->sample_direction(origin);
}

} // namespace pt
