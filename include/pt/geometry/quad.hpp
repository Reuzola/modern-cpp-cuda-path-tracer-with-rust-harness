#pragma once
#include "pt/core/hit_record.hpp"
#include "pt/core/hittable.hpp"
#include "pt/core/sampleable.hpp"
#include "pt/math/aabb.hpp"
#include "pt/math/interval.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include <type_traits>

namespace pt {

class Material;

// What a quad's intersection test reads: corner, edges, and the plane and inverse basis
// derived from them once. Trivially copyable for the same reason as SphereShape.
struct QuadShape {
    Point3 q;
    Vec3 u;
    Vec3 v;
    Vec3 normal; // Unit
    Float d{};   // Plane offset, normal . q
    Vec3 w;      // n / (n . n) with n = u x v: recovers (alpha, beta) without forming a matrix
};

static_assert(std::is_trivially_copyable_v<QuadShape>);

// Pure geometric result of the quad test: distance, and the hit's coordinates along u_ and v_.
struct QuadHit {
    Float t{};
    Float alpha{};
    Float beta{};
};

[[nodiscard]] QuadShape make_quad_shape(const Point3& q, const Vec3& u, const Vec3& v) noexcept;

[[nodiscard]] Aabb quad_bounds(const QuadShape& quad) noexcept;

// Geometric test only; writes out on a hit and leaves it untouched otherwise.
[[nodiscard]] bool intersect_quad(const Ray& r, const Interval& ray_t, const QuadShape& quad, QuadHit& out) noexcept;

// Everything a hit carries beyond its distance, for the one candidate that won.
void fill_quad_record(const Ray& r, const QuadShape& quad, const QuadHit& hit, const Material* mat, HitRecord& rec) noexcept;

class Quad final : public Hittable, public Sampleable {
public:
    Quad(const Point3& q, const Vec3& u, const Vec3& v, const Material* mat);

    [[nodiscard]] bool hit(const Ray& r, const Interval& ray_t, HitRecord& rec) const override;

    [[nodiscard]] Aabb bounding_box() const override;

    [[nodiscard]] Float pdf_direction(const Point3& origin, const Vec3& direction) const override;

    [[nodiscard]] Vec3 sample_direction(const Point3& origin, Sampler& sampler) const override;

    [[nodiscard]] const QuadShape& shape() const noexcept { return shape_; }

    [[nodiscard]] const Material* material() const noexcept { return mat_; }

private:
    QuadShape shape_;
    const Material* mat_ = nullptr;
    Aabb bbox_;
    Float area_{}; // Light sampling only: turns the area density into a solid-angle one.
};

} // namespace pt
