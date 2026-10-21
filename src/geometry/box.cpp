#include "pt/geometry/box.hpp"
#include "pt/core/hittable.hpp"
#include "pt/geometry/bvh.hpp"
#include "pt/geometry/quad.hpp"
#include "pt/geometry/shape_bvh.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include "pt/util/arena.hpp"
#include <cmath>

namespace pt {

namespace {

// One leaf: the faces are tested in a row, as a box always was. A depth limit of one spends the
// root's budget at once, so the builder stops there without consulting the SAH. Whether six
// faces deserve a tree is the cost sweep's question, not this file's.
constexpr BvhBuildSettings box_settings{.max_leaf_size = 6, .depth_limit = 1};

} // namespace

BoxFaces box(Arena<Hittable>& arena, const Point3& a, const Point3& b, const Material* mat) {
    const Point3 min_corner(std::fmin(a.x(), b.x()), std::fmin(a.y(), b.y()), std::fmin(a.z(), b.z()));
    const Point3 max_corner(std::fmax(a.x(), b.x()), std::fmax(a.y(), b.y()), std::fmax(a.z(), b.z()));

    const Vec3 dx = Vec3(max_corner.x() - min_corner.x(), 0.0_f, 0.0_f);
    const Vec3 dy = Vec3(0.0_f, max_corner.y() - min_corner.y(), 0.0_f);
    const Vec3 dz = Vec3(0.0_f, 0.0_f, max_corner.z() - min_corner.z());

    Point3 q;
    Vec3 u;
    Vec3 v;

    q = Point3(min_corner.x(), min_corner.y(), max_corner.z()); // front face
    u = dx;
    v = dy;
    const Quad* front = arena.create<Quad>(q, u, v, mat);

    q = Point3(max_corner.x(), min_corner.y(), min_corner.z()); // right face
    u = dy;
    v = dz;
    const Quad* right = arena.create<Quad>(q, u, v, mat);

    q = Point3(min_corner.x(), min_corner.y(), min_corner.z()); // rear face
    u = dy;
    v = dx;
    const Quad* rear = arena.create<Quad>(q, u, v, mat);

    q = Point3(min_corner.x(), min_corner.y(), min_corner.z()); // left face
    u = dz;
    v = dy;
    const Quad* left = arena.create<Quad>(q, u, v, mat);

    q = Point3(min_corner.x(), max_corner.y(), min_corner.z()); // top face
    u = dz;
    v = dx;
    const Quad* top = arena.create<Quad>(q, u, v, mat);

    q = Point3(min_corner.x(), min_corner.y(), min_corner.z()); // bottom face
    u = dx;
    v = dz;
    const Quad* bottom = arena.create<Quad>(q, u, v, mat);

    return BoxFaces{.faces = {front, right, rear, left, top, bottom}};
}

const QuadBvh* make_box_bvh(Arena<Hittable>& arena, const BoxFaces& box, BvhStats* stats) {
    return make_shape_bvh<Quad>(arena, box.faces, stats, box_settings);
}

} // namespace pt
