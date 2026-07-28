#pragma once
#include "pt/core/hit_record.hpp"
#include "pt/core/hittable.hpp"
#include "pt/math/aabb.hpp"
#include "pt/math/interval.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/vec3.hpp"
#include <memory>
#include <vector>

namespace pt {

class hittable_list final : public hittable {
    friend class bvh_node;

public:
    hittable_list() = default;
    explicit hittable_list(std::shared_ptr<hittable> object);

    [[nodiscard]] aabb bounding_box() const override;

    [[nodiscard]] bool hit(const ray& r, const interval& ray_t, hit_record& rec) const override;

    [[nodiscard]] double pdf_value(const point3& origin, const vec3& direction) const override;

    [[nodiscard]] vec3 random(const point3& origin) const override;

    void clear();

    void add(std::shared_ptr<hittable> obj);

private:
    std::vector<std::shared_ptr<hittable>> objects;
    aabb bbox;
};

} // namespace pt
