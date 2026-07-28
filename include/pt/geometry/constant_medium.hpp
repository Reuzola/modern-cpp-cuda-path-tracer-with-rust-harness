#pragma once
#include "pt/core/hit_record.hpp"
#include "pt/core/hittable.hpp"
#include "pt/materials/material.hpp"
#include "pt/math/aabb.hpp"
#include "pt/math/interval.hpp"
#include <cmath>
#include <memory>

namespace pt {

class ray;

class constant_medium : public hittable {
public:
    constant_medium(std::shared_ptr<hittable> boundary, double density, const material* phase_function);

    [[nodiscard]] aabb bounding_box() const override;

    [[nodiscard]] bool hit(const ray& r, const interval& ray_t, hit_record& rec) const override;

private:
    std::shared_ptr<hittable> boundary;
    double neg_inv_density{};
    const material* phase_function;
};

} // namespace pt
