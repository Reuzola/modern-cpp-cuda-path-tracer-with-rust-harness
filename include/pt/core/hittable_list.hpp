#pragma once
#include "pt/core/hit_record.hpp"
#include "pt/core/hittable.hpp"
#include "pt/math/aabb.hpp"
#include "pt/math/interval.hpp"
#include "pt/math/ray.hpp"
#include <span>
#include <vector>

namespace pt {

class HittableList final : public Hittable {
public:
    HittableList() = default;
    explicit HittableList(const Hittable* object);

    [[nodiscard]] Aabb bounding_box() const override;

    [[nodiscard]] bool hit(const Ray& r, const Interval& ray_t, HitRecord& rec) const override;

    void clear();

    void add(const Hittable* obj);

    [[nodiscard]] bool empty() const noexcept { return objects_.empty(); }

    [[nodiscard]] std::span<const Hittable* const> objects() const noexcept { return objects_; }

private:
    std::vector<const Hittable*> objects_;
    Aabb bbox_;
};

} // namespace pt
