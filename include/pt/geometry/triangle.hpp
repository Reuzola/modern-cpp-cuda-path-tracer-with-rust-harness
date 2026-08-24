#pragma once
#include "pt/core/hit_record.hpp"
#include "pt/core/hittable.hpp"
#include "pt/math/aabb.hpp"
#include "pt/math/interval.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"

namespace pt {

class Material;

// Pure geometric result of a ray-triangle test. Barycentrics are relative to v0: the weight of v0 is 1 - b1 - b2.
struct TriangleHit {
    Float t{};
    Float b1{};
    Float b2{};
    Vec3 normal;
};

[[nodiscard]] bool intersect_triangle(const Ray& r, const Interval& ray_t, const Point3& v0,
                                      const Point3& v1, const Point3& v2, TriangleHit& out) noexcept;

// Component-wise min/max over all three vertices. Combining pairwise boxes instead would leak an
// intermediate box's degenerate-axis padding into the result, making the bounds depend on vertex order.
[[nodiscard]] Aabb triangle_bounds(const Point3& v0, const Point3& v1, const Point3& v2) noexcept;

class Triangle final : public Hittable {
public:
    Triangle(const Point3& v0, const Point3& v1, const Point3& v2, const Material* mat);

    [[nodiscard]] bool hit(const Ray& r, const Interval& ray_t, HitRecord& rec) const override;

    [[nodiscard]] Aabb bounding_box() const override;

private:
    Point3 v0_, v1_, v2_;
    const Material* mat_ = nullptr;
    Aabb bbox_;
};

} // namespace pt
