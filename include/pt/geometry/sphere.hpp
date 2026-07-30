#pragma once
#include "pt/core/hit_record.hpp"
#include "pt/core/hittable.hpp"
#include "pt/core/sampleable.hpp"
#include "pt/math/aabb.hpp"
#include "pt/math/interval.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"

namespace pt {

class Material;

class Sphere final : public Hittable, public Sampleable {
public:
    Sphere(const Point3& center1, const Point3& center2, Float radius, const Material* mat);

    Sphere(const Point3& static_center, Float radius, const Material* mat);

    [[nodiscard]] Aabb bounding_box() const override;

    [[nodiscard]] bool hit(const Ray& r, const Interval& ray_t, HitRecord& rec) const override;

    [[nodiscard]] Float pdf_direction(const Point3& origin, const Vec3& direction) const override;

    [[nodiscard]] Vec3 sample_direction(const Point3& origin) const override;

private:
    Ray center_;
    Float radius_;
    const Material* mat_ = nullptr;
    Aabb bbox_;

    struct UvCoords {
        Float u{}, v{};
    };

    [[nodiscard]] static UvCoords get_sphere_uv(const Point3& p);

    [[nodiscard]] static Vec3 random_to_sphere(Float radius, Float distance_squared);
};

} // namespace pt
