#pragma once
#include "pt/core/hit_record.hpp"
#include "pt/core/hittable.hpp"
#include "pt/math/aabb.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/scalar.hpp"

namespace pt {

class Interval;

class Sampler;

class RotateY final : public Hittable {
public:
    RotateY(const Hittable* object, Float angle);

    [[nodiscard]] Aabb bounding_box() const override;

    [[nodiscard]] bool hit(const Ray& r, const Interval& ray_t, HitRecord& rec, Sampler& sampler) const override;

private:
    const Hittable* object_;
    Float sin_theta_{};
    Float cos_theta_{};
    Aabb bbox_;
};

} // namespace pt
