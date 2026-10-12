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

class Quad final : public Hittable, public Sampleable {
public:
    Quad(const Point3& q, const Vec3& u, const Vec3& v, const Material* mat);

    [[nodiscard]] bool hit(const Ray& r, const Interval& ray_t, HitRecord& rec) const override;

    [[nodiscard]] Aabb bounding_box() const override;

    [[nodiscard]] Float pdf_direction(const Point3& origin, const Vec3& direction) const override;

    [[nodiscard]] Vec3 sample_direction(const Point3& origin, Sampler& sampler) const override;

private:
    Point3 q_;
    Vec3 u_, v_;
    const Material* mat_ = nullptr;
    Aabb bbox_;
    Vec3 normal_;
    Float d_{};
    Vec3 w_;
    Float area_{};

    void set_bounding_box();

    // Pure geometric result of the quad test: distance, and the hit's coordinates along u_ and v_.
    struct QuadHit {
        Float t{};
        Float alpha{};
        Float beta{};
    };

    [[nodiscard]] static bool is_interior(Float a, Float b) noexcept;

    // Geometric test only; writes out on a hit and leaves it untouched otherwise.
    [[nodiscard]] bool find_hit(const Ray& r, const Interval& ray_t, QuadHit& out) const noexcept;

    // Everything a hit carries beyond its distance; run once per accepted root.
    void fill_record(const Ray& r, const QuadHit& quad_hit, HitRecord& rec) const noexcept;
};

} // namespace pt
