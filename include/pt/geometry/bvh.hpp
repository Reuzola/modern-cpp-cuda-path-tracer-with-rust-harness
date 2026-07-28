#pragma once
#include "pt/core/hit_record.hpp"
#include "pt/core/hittable.hpp"
#include "pt/core/hittable_list.hpp"
#include "pt/math/aabb.hpp"
#include "pt/math/interval.hpp"
#include <memory>
#include <span>

namespace pt {

class ray;

class bvh_node : public hittable {
public:
    bvh_node(hittable_list list);

    bvh_node(std::span<std::shared_ptr<hittable>> objects);

    [[nodiscard]] bool hit(const ray& r, const interval& ray_t, hit_record& rec) const override;

    [[nodiscard]] aabb bounding_box() const override;

private:
    std::shared_ptr<hittable> left;
    std::shared_ptr<hittable> right;
    aabb bbox;

    static bool box_compare(const std::shared_ptr<hittable>& a, const std::shared_ptr<hittable>& b, int axis_index);

    static bool box_x_compare(const std::shared_ptr<hittable>& a, const std::shared_ptr<hittable>& b);
    static bool box_y_compare(const std::shared_ptr<hittable>& a, const std::shared_ptr<hittable>& b);
    static bool box_z_compare(const std::shared_ptr<hittable>& a, const std::shared_ptr<hittable>& b);
};

} // namespace pt
