#pragma once
#include "pt/math/ray.hpp"
#include "pt/math/robust.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"

namespace pt {

class Material;

struct HitRecord {
    Point3 p;
    Vec3 normal;

    // Geometric normal, flipped with the shading one. Zero for volume interactions,
    // which have no surface to escape and must spawn from the sample point itself.
    Vec3 geometric_normal;
    Float t{};
    Float u{};
    Float v{};
    bool front_face{};
    const Material* mat = nullptr;

    // Orientation is decided by the geometric normal; both normals are then stored
    // facing the ray. An interpolated normal near a silhouette can face the ray even
    // where the surface itself does not, and the offset must follow the surface.
    void constexpr set_face_normal(const Ray& r, const Vec3& outward_geometric, const Vec3& outward_shading) noexcept {
        front_face = dot(r.direction(), outward_geometric) < 0;
        normal = front_face ? outward_shading : -outward_shading;
        geometric_normal = front_face ? outward_geometric : -outward_geometric;
    }

    void constexpr set_face_normal(const Ray& r, const Vec3& outward_normal) noexcept {
        set_face_normal(r, outward_normal, outward_normal);
    }

    // Spawns from a point offset off the surface, on the side the direction leaves
    // toward: the query range is then a plain [0, inf) with no epsilon in it.
    [[nodiscard]] constexpr Ray spawn_ray(const Vec3& direction, Float time) const noexcept {
        const Vec3 offset_side = dot(direction, geometric_normal) < 0 ? -geometric_normal : geometric_normal;
        const Point3 origin = offset_ray_origin(p, offset_side);
        return Ray(origin, direction, time);
    }
};

// Contract guards: these fail the build if a member below silently loses constexpr.
static_assert([] {
    HitRecord rec;
    rec.set_face_normal(Ray(Point3(0, 0, 0), Vec3(0, 0, -1)), Vec3(0, 0, 1));
    return rec.front_face;
}());

static_assert([] {
    HitRecord rec;
    rec.p = Point3(1.5_f, -2.0_f, 3.25_f);
    rec.geometric_normal = Vec3(0, 0, 0);
    const Ray r = rec.spawn_ray(Vec3(0, 1, 0), 0.0_f);
    return r.origin().x() == rec.p.x() && r.origin().y() == rec.p.y() && r.origin().z() == rec.p.z();
}());

static_assert([] {
    HitRecord rec;
    rec.p = Point3(1, 2, 3);
    rec.geometric_normal = Vec3(0, 0, 1);
    const Ray r = rec.spawn_ray(Vec3(0, 0, -1), 0.0_f);
    return dot(r.origin() - rec.p, rec.geometric_normal) < 0.0_f;
}());

} // namespace pt
