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

    void set_face_normal(const Ray& r, const Vec3& outward_normal) {
        front_face = dot(r.direction(), outward_normal) < 0;
        normal = front_face ? outward_normal : -outward_normal;
    }
};

} // namespace pt
