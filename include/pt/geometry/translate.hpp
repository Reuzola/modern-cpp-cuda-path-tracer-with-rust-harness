#pragma once
#include "pt/core/hit_record.hpp"
#include "pt/core/hittable.hpp"
#include "pt/math/aabb.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/vec3.hpp"

namespace pt {

class Interval;

class Translate final : public Hittable {
public:
    Translate(const Hittable* object, const Vec3& offset);

    [[nodiscard]] Aabb bounding_box() const override;

    [[nodiscard]] bool hit(const Ray& r, const Interval& ray_t, HitRecord& rec) const override;

private:
    const Hittable* object_;
    Vec3 offset_;
    Aabb bbox_;
};

} // namespace pt
