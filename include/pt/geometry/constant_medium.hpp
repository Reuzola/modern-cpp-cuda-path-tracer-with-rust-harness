#pragma once
#include "pt/math/scalar.hpp"

namespace pt {

class Hittable;
class Material;
class Ray;
class Sampler;
class Interval;
struct HitRecord;

class ConstantMedium final {
public:
    ConstantMedium(const Hittable* boundary, Float density, const Material* phase_function);

    [[nodiscard]] bool sample_interaction(const Ray& r, const Interval& ray_t, Sampler& sampler, HitRecord& rec) const;

private:
    const Hittable* boundary_;
    Float neg_inv_density_{};
    const Material* phase_function_;
};

} // namespace pt
