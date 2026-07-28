#pragma once
#include "pt/core/hit_record.hpp"
#include "pt/core/hittable.hpp"
#include "pt/math/aabb.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/vec3.hpp"
#include <memory>

namespace pt {

class interval;

class translate : public hittable {
public:
    translate(std::shared_ptr<hittable> object, const vec3& offset);

    [[nodiscard]] aabb bounding_box() const override;

    [[nodiscard]] bool hit(const ray& r, const interval& ray_t, hit_record& rec) const override;

private:
    std::shared_ptr<hittable> object;
    vec3 offset;
    aabb bbox;
};

} // namespace pt
