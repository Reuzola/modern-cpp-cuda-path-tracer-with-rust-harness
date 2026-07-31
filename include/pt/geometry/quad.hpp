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

class Sampler;

class Quad final : public Hittable, public Sampleable {
public:
    Quad(const Point3& Q, const Vec3& u, const Vec3& v, const Material* mat);

    [[nodiscard]] bool hit(const Ray& r, const Interval& ray_t, HitRecord& rec) const override;

    [[nodiscard]] Aabb bounding_box() const override;

    [[nodiscard]] Float pdf_direction(const Point3& origin, const Vec3& direction) const override;

    [[nodiscard]] Vec3 sample_direction(const Point3& origin, Sampler& sampler) const override;

private:
    Point3 Q_;
    Vec3 u_, v_;
    const Material* mat_ = nullptr;
    Aabb bbox_;
    Vec3 normal_;
    Float D_{};
    Vec3 w_;
    Float area_{};

    void set_bounding_box();

    [[nodiscard]] bool is_interior(Float a, Float b, HitRecord& rec) const;
};

} // namespace pt
