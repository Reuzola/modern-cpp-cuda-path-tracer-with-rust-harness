#pragma once
#include "pt/geometry/top_level_bvh.hpp"
#include "pt/math/scalar.hpp"

namespace pt {

class Material;
class Ray;
class Sampler;
class Interval;
struct HitRecord;

class ConstantMedium final {
public:
    // The boundary is a tree of its own, never part of the world: a volume is not a surface.
    ConstantMedium(TopLevelBvh boundary, Float density, const Material* phase_function);

    [[nodiscard]] bool sample_interaction(const Ray& r, const Interval& ray_t, Sampler& sampler, HitRecord& rec) const;

private:
    TopLevelBvh boundary_;
    Float neg_inv_density_{};
    const Material* phase_function_;
};

} // namespace pt
