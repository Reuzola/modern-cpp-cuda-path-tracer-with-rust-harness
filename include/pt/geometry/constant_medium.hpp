#pragma once
#include "pt/core/hit_record.hpp"
#include "pt/core/hittable.hpp"
#include "pt/math/aabb.hpp"
#include "pt/math/interval.hpp"
#include "pt/math/scalar.hpp"

namespace pt {

class Material;

class Ray;

class Sampler;

class ConstantMedium final : public Hittable {
public:
    ConstantMedium(const Hittable* boundary, Float density, const Material* phase_function);

    [[nodiscard]] Aabb bounding_box() const override;

    [[nodiscard]] bool hit(const Ray& r, const Interval& ray_t, HitRecord& rec, Sampler& sampler) const override;

private:
    const Hittable* boundary_;
    Float neg_inv_density_{};
    const Material* phase_function_;
};

} // namespace pt
