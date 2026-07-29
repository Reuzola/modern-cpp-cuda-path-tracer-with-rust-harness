#pragma once
#include "pt/core/hit_record.hpp"
#include "pt/core/hittable.hpp"
#include "pt/core/hittable_list.hpp"
#include "pt/math/aabb.hpp"
#include "pt/math/interval.hpp"
#include <memory>
#include <span>

namespace pt {

class Ray;

class BvhNode final : public Hittable {
public:
    BvhNode(HittableList list);

    BvhNode(std::span<std::shared_ptr<Hittable>> objects);

    [[nodiscard]] bool hit(const Ray& r, const Interval& ray_t, HitRecord& rec) const override;

    [[nodiscard]] Aabb bounding_box() const override;

private:
    std::shared_ptr<Hittable> left_;
    std::shared_ptr<Hittable> right_;
    Aabb bbox_;

    static bool box_compare(const std::shared_ptr<Hittable>& a, const std::shared_ptr<Hittable>& b, int axis_index);

    static bool box_x_compare(const std::shared_ptr<Hittable>& a, const std::shared_ptr<Hittable>& b);
    static bool box_y_compare(const std::shared_ptr<Hittable>& a, const std::shared_ptr<Hittable>& b);
    static bool box_z_compare(const std::shared_ptr<Hittable>& a, const std::shared_ptr<Hittable>& b);
};

} // namespace pt
