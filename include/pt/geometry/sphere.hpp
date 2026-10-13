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

namespace pt {

class Material;

class Sphere final : public Hittable, public Sampleable {
public:
    Sphere(const Point3& center1, const Point3& center2, Float radius, const Material* mat);

    Sphere(const Point3& static_center, Float radius, const Material* mat);

    [[nodiscard]] Aabb bounding_box() const override;

    [[nodiscard]] bool hit(const Ray& r, const Interval& ray_t, HitRecord& rec) const override;

    [[nodiscard]] Float pdf_direction(const Point3& origin, const Vec3& direction) const override;

    [[nodiscard]] Vec3 sample_direction(const Point3& origin, Sampler& sampler) const override;

private:
    Ray center_;
    Float radius_;
    const Material* mat_ = nullptr;
    Aabb bbox_;
    Float spawn_margin_{}; // Fixed per sphere: proportional to the radius, never to the hit point.

    struct UvCoords {
        Float u{}, v{};
    };

    [[nodiscard]] static UvCoords get_sphere_uv(const Point3& p);

    // 1 - cos(theta_max) of the cone a sphere subtends, shared by the
    // density and the sampler so the two describe one cone.
    [[nodiscard]] static Float cone_one_minus_cos(Float radius, Float distance_squared) noexcept;

    [[nodiscard]] static Vec3 random_to_sphere(Float radius, Float distance_squared, Sampler& sampler);

    // Geometric test only: the nearest root inside ray_t. Static and handed
    // its data, so the light pdf can ask hit-or-miss without paying for the record.
    [[nodiscard]] static std::optional<Float> find_root(const Ray& r, const Point3& center, Float radius, const Interval& ray_t) noexcept;

    // Everything a hit carries beyond its distance; run once per accepted root.
    void fill_record(const Ray& r, const Point3& center, Float t, HitRecord& rec) const noexcept;
};

} // namespace pt
