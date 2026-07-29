#pragma once
#include "pt/core/hit_record.hpp"
#include "pt/core/hittable.hpp"
#include "pt/math/aabb.hpp"
#include "pt/math/interval.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include <memory>
#include <vector>

namespace pt {

class HittableList final : public Hittable {
    friend class BvhNode;

public:
    HittableList() = default;
    explicit HittableList(std::shared_ptr<Hittable> object);

    [[nodiscard]] Aabb bounding_box() const override;

    [[nodiscard]] bool hit(const Ray& r, const Interval& ray_t, HitRecord& rec) const override;

    [[nodiscard]] Float pdf_value(const Point3& origin, const Vec3& direction) const override;

    [[nodiscard]] Vec3 random(const Point3& origin) const override;

    void clear();

    void add(std::shared_ptr<Hittable> obj);

private:
    std::vector<std::shared_ptr<Hittable>> objects;
    Aabb bbox;
};

} // namespace pt
