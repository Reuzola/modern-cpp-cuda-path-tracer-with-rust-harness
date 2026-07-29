#pragma once
#include "pt/core/hit_record.hpp"
#include "pt/core/hittable.hpp"
#include "pt/math/aabb.hpp"
#include "pt/math/interval.hpp"
#include "pt/math/scalar.hpp"
#include <memory>

namespace pt {

class material;

class ray;

class constant_medium final : public hittable {
public:
    constant_medium(std::shared_ptr<hittable> boundary, Float density, const material* phase_function);

    [[nodiscard]] aabb bounding_box() const override;

    [[nodiscard]] bool hit(const ray& r, const interval& ray_t, hit_record& rec) const override;

private:
    std::shared_ptr<hittable> boundary;
    Float neg_inv_density{};
    const material* phase_function;
};

} // namespace pt
