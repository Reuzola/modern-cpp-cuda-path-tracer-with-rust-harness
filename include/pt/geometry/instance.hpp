#pragma once
#include "pt/core/hit_record.hpp"
#include "pt/core/hittable.hpp"
#include "pt/math/aabb.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/transform.hpp"

namespace pt {

class Interval;

// A transformed reference to another hittable. The child is shared rather than
// owned, so the same geometry can appear under any number of instances.
class Instance final : public Hittable {
public:
    Instance(const Hittable* object, const Transform& transform);

    [[nodiscard]] bool hit(const Ray& r, const Interval& ray_t, HitRecord& rec) const override;

    [[nodiscard]] Aabb bounding_box() const override;

private:
    const Hittable* object_;
    Transform transform_;
    Aabb bbox_;
};

} // namespace pt
