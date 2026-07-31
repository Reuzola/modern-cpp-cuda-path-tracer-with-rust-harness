#pragma once
#include "pt/core/hit_record.hpp"
#include "pt/core/hittable.hpp"
#include "pt/core/hittable_list.hpp"
#include "pt/math/aabb.hpp"
#include "pt/math/interval.hpp"
#include "pt/util/arena.hpp"
#include <span>

namespace pt {

class Ray;

class Sampler;

class BvhNode final : public Hittable {
public:
    BvhNode(Arena<Hittable>& arena, const HittableList& list);

    BvhNode(Arena<Hittable>& arena, std::span<const Hittable*> objects);

    [[nodiscard]] bool hit(const Ray& r, const Interval& ray_t, HitRecord& rec, Sampler& sampler) const override;

    [[nodiscard]] Aabb bounding_box() const override;

private:
    const Hittable* left_ = nullptr;
    const Hittable* right_ = nullptr;
    Aabb bbox_;

    void build(Arena<Hittable>& arena, std::span<const Hittable*> objects);

    static bool box_compare(const Hittable* a, const Hittable* b, int axis_index);

    static bool box_x_compare(const Hittable* a, const Hittable* b);
    static bool box_y_compare(const Hittable* a, const Hittable* b);
    static bool box_z_compare(const Hittable* a, const Hittable* b);
};

} // namespace pt
