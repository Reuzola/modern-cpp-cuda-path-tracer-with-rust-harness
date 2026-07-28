#pragma once
#include "pt/core/hit_record.hpp"
#include "pt/core/hittable.hpp"
#include "pt/math/aabb.hpp"
#include "pt/math/ray.hpp"
#include <memory>

namespace pt {

class interval;

class rotate_y final : public hittable {
public:
    rotate_y(std::shared_ptr<hittable> object, double angle);

    [[nodiscard]] aabb bounding_box() const override;

    [[nodiscard]] bool hit(const ray& r, const interval& ray_t, hit_record& rec) const override;

private:
    std::shared_ptr<hittable> object;
    double sin_theta{};
    double cos_theta{};
    aabb bbox;
};

} // namespace pt
