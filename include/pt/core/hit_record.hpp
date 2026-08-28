#pragma once
#include "pt/math/ray.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"

namespace pt {

class Material;

struct HitRecord {
    Point3 p;
    Vec3 normal;
    Float t{};
    Float u{};
    Float v{};
    bool front_face{};
    const Material* mat = nullptr;

    // Orientation is decided by the geometric normal, but the stored normal is the
    // shading one, flipped to match: an interpolated normal near a silhouette can
    // face the ray even where the surface itself does not.
    void constexpr set_face_normal(const Ray& r, const Vec3& geometric_normal, const Vec3& shading_normal) noexcept {
        front_face = dot(r.direction(), geometric_normal) < 0;
        normal = front_face ? shading_normal : -shading_normal;
    }

    void constexpr set_face_normal(const Ray& r, const Vec3& outward_normal) noexcept {
        set_face_normal(r, outward_normal, outward_normal);
    }
};

// Contract guards: these fail the build if a member below silently loses constexpr.
static_assert([] {
    HitRecord rec;
    rec.set_face_normal(Ray(Point3(0, 0, 0), Vec3(0, 0, -1)), Vec3(0, 0, 1));
    return rec.front_face;
}());

} // namespace pt
