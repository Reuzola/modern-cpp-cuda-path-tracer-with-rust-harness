#include "pt/geometry/box.hpp"
#include "pt/core/hittable.hpp"
#include "pt/core/hittable_list.hpp"
#include "pt/geometry/quad.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include "pt/util/arena.hpp"
#include <cmath>
#include <utility>

namespace pt {

const HittableList* box(Arena<Hittable>& arena, const Point3& a, const Point3& b, const Material* mat) {
    const Point3 min_corner(std::fmin(a.x(), b.x()), std::fmin(a.y(), b.y()), std::fmin(a.z(), b.z()));
    const Point3 max_corner(std::fmax(a.x(), b.x()), std::fmax(a.y(), b.y()), std::fmax(a.z(), b.z()));

    const Vec3 dx = Vec3(max_corner.x() - min_corner.x(), 0.0_f, 0.0_f);
    const Vec3 dy = Vec3(0.0_f, max_corner.y() - min_corner.y(), 0.0_f);
    const Vec3 dz = Vec3(0.0_f, 0.0_f, max_corner.z() - min_corner.z());

    HittableList sides;
    Point3 q;
    Vec3 u;
    Vec3 v;

    q = Point3(min_corner.x(), min_corner.y(), max_corner.z()); // front face
    u = dx;
    v = dy;
    sides.add(arena.create<Quad>(q, u, v, mat));

    q = Point3(max_corner.x(), min_corner.y(), min_corner.z()); // right face
    u = dy;
    v = dz;
    sides.add(arena.create<Quad>(q, u, v, mat));

    q = Point3(min_corner.x(), min_corner.y(), min_corner.z()); // rear face
    u = dy;
    v = dx;
    sides.add(arena.create<Quad>(q, u, v, mat));

    q = Point3(min_corner.x(), min_corner.y(), min_corner.z()); // left face
    u = dz;
    v = dy;
    sides.add(arena.create<Quad>(q, u, v, mat));

    q = Point3(min_corner.x(), max_corner.y(), min_corner.z()); // top face
    u = dz;
    v = dx;
    sides.add(arena.create<Quad>(q, u, v, mat));

    q = Point3(min_corner.x(), min_corner.y(), min_corner.z()); // bottom face
    u = dx;
    v = dz;
    sides.add(arena.create<Quad>(q, u, v, mat));

    return arena.create<HittableList>(std::move(sides));
}

} // namespace pt
