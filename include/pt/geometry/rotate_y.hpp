#pragma once
#include "pt/core/hit_record.hpp"
#include "pt/core/hittable.hpp"
#include "pt/math/aabb.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/scalar.hpp"
#include <memory>

namespace pt {

class Interval;

class RotateY final : public Hittable {
public:
    RotateY(std::shared_ptr<Hittable> object, Float angle);

    [[nodiscard]] Aabb bounding_box() const override;

    [[nodiscard]] bool hit(const Ray& r, const Interval& ray_t, HitRecord& rec) const override;

private:
    std::shared_ptr<Hittable> object;
    Float sin_theta{};
    Float cos_theta{};
    Aabb bbox;
};

} // namespace pt
