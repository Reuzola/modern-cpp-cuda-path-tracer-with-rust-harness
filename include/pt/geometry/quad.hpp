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

class Quad final : public Hittable {
public:
    Quad(const Point3& Q, const Vec3& u, const Vec3& v, const Material* mat);

    [[nodiscard]] bool hit(const Ray& r, const Interval& ray_t, HitRecord& rec) const override;

    [[nodiscard]] Aabb bounding_box() const override;

    [[nodiscard]] Float pdf_value(const Point3& origin, const Vec3& direction) const override;

    [[nodiscard]] Vec3 random(const Point3& origin) const override;

private:
    Point3 Q;
    Vec3 u, v;
    const Material* mat = nullptr;
    Aabb bbox;
    Vec3 normal;
    Float D{};
    Vec3 w;
    Float area{};

    void set_bounding_box();

    [[nodiscard]] bool is_interior(Float a, Float b, HitRecord& rec) const;
};

} // namespace pt
