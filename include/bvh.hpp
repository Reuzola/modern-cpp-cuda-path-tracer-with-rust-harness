#pragma once
#include "aabb.hpp"
#include "hit_record.hpp"
#include "hittable.hpp"
#include "hittable_list.hpp"
#include "interval.hpp"
#include "random.hpp"
#include <algorithm>
#include <memory>
#include <span>

class bvh_node : public hittable {
    public:
        bvh_node(hittable_list list) : bvh_node(std::span<std::shared_ptr<hittable>>(list.objects)) {}
        bvh_node(std::span<std::shared_ptr<hittable>> objects) {
            int axis = random_int(0, 2);

            auto comparator = (axis == 0) ? box_x_compare : (axis == 1) ? box_y_compare : box_z_compare;
            const auto count = objects.size();

            if (count == 1) {
                left = right = objects[0];
            } else if (count == 2) {
                left = objects[0];
                right = objects[1];
            } else {
                std::sort(objects.begin(), objects.end(), comparator);
                const auto mid = count / 2;

                left = std::make_shared<bvh_node>(objects.subspan(0, mid));
                right = std::make_shared<bvh_node>(objects.subspan(mid));
            }
            bbox = aabb(left->bounding_box(), right->bounding_box());
        }

        [[nodiscard]] bool hit(const ray& r, const interval& ray_t, hit_record& rec) const override {
            if (!bbox.hit(r, ray_t)) return false;

            bool hit_left = left->hit(r, ray_t, rec);
            bool hit_right = right->hit(r, interval(ray_t.min, hit_left ? rec.t : ray_t.max), rec);

            return hit_left || hit_right;
        }

        [[nodiscard]] aabb bounding_box() const override { return bbox; }
    private:
        std::shared_ptr<hittable> left;
        std::shared_ptr<hittable> right;
        aabb bbox;

        static bool box_compare(const std::shared_ptr<hittable>& a, const std::shared_ptr<hittable>& b, int axis_index) {
            const auto a_axis_interval = a->bounding_box().axis_interval(axis_index);
            const auto b_axis_interval = b->bounding_box().axis_interval(axis_index);
            return a_axis_interval.min < b_axis_interval.min;
        }

        static bool box_x_compare(const std::shared_ptr<hittable>& a, const std::shared_ptr<hittable>& b) { return box_compare(a, b, 0); }
        static bool box_y_compare(const std::shared_ptr<hittable>& a, const std::shared_ptr<hittable>& b) { return box_compare(a, b, 1); }
        static bool box_z_compare(const std::shared_ptr<hittable>& a, const std::shared_ptr<hittable>& b) { return box_compare(a, b, 2); }
};