#pragma once
#include "pt/core/hit_record.hpp"
#include "pt/core/hittable.hpp"
#include "pt/core/sampleable.hpp"
#include "pt/math/aabb.hpp"
#include "pt/math/interval.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include <optional>
#include <type_traits>

namespace pt {

class Material;

// What a sphere's intersection test reads, and all it reads: trivially copyable, so a set can
// lay these out in leaf order and a device buffer can hold them as they are.
struct SphereShape {
    Point3 center; // At shutter open
    Vec3 motion;   // Travel over the shutter interval; zero for a static sphere
    Float radius{};

    // Where a ray at `time` finds the centre; the bounds sweep it over [0, 1].
    [[nodiscard]] constexpr Point3 center_at(Float time) const noexcept { return center + time * motion; }
};

static_assert(std::is_trivially_copyable_v<SphereShape>);

// Swept over the whole shutter interval: one tree serves every time a ray may carry.
[[nodiscard]] Aabb sphere_bounds(const SphereShape& sphere) noexcept;

// Geometric test only: the nearest root inside ray_t, at the centre the ray's time selects.
[[nodiscard]] std::optional<Float> intersect_sphere(const Ray& r, const Interval& ray_t, const SphereShape& sphere) noexcept;

// Everything a hit carries beyond its distance, for the one candidate that won.
void fill_sphere_record(const Ray& r, const SphereShape& sphere, Float t, const Material* mat, HitRecord& rec) noexcept;

// One sphere and its material: a scene object, and a target for light sampling.
class Sphere final : public Hittable, public Sampleable {
public:
    Sphere(const Point3& center1, const Point3& center2, Float radius, const Material* mat);

    Sphere(const Point3& static_center, Float radius, const Material* mat);

    [[nodiscard]] Aabb bounding_box() const override;

    [[nodiscard]] bool hit(const Ray& r, const Interval& ray_t, HitRecord& rec) const override;

    [[nodiscard]] Float pdf_direction(const Point3& origin, const Vec3& direction) const override;

    [[nodiscard]] Vec3 sample_direction(const Point3& origin, Sampler& sampler) const override;

    [[nodiscard]] const SphereShape& shape() const noexcept { return shape_; }

    [[nodiscard]] const Material* material() const noexcept { return mat_; }

private:
    SphereShape shape_;
    const Material* mat_ = nullptr;
    Aabb bbox_;
};

} // namespace pt
