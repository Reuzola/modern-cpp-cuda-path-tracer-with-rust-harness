#pragma once
#include "hittable_list.hpp"
#include "quad.hpp"
#include "vec3.hpp"
#include <cmath>
#include <memory>

class material;

[[nodiscard]] inline std::shared_ptr<hittable_list> box(const point3& a, const point3& b, const material* mat) {
    const point3 min_corner(std::fmin(a.x(), b.x()), std::fmin(a.y(), b.y()), std::fmin(a.z(), b.z()));
    const point3 max_corner(std::fmax(a.x(), b.x()), std::fmax(a.y(), b.y()), std::fmax(a.z(), b.z()));

    const vec3 dx = vec3(max_corner.x() - min_corner.x(), 0.0, 0.0);
    const vec3 dy = vec3(0.0, max_corner.y() - min_corner.y(), 0.0);
    const vec3 dz = vec3(0.0, 0.0, max_corner.z() - min_corner.z());

    auto sides = std::make_shared<hittable_list>();
    point3 Q;
    vec3 u, v;

    Q = point3(min_corner.x(), min_corner.y(), max_corner.z()); // front face
    u = dx;
    v = dy;
    sides->add(std::make_shared<quad>(Q, u, v, mat));

    Q = point3(max_corner.x(), min_corner.y(), min_corner.z()); // right face
    u = dy;
    v = dz;
    sides->add(std::make_shared<quad>(Q, u, v, mat));

    Q = point3(min_corner.x(), min_corner.y(), min_corner.z()); // rear face
    u = dy;
    v = dx;
    sides->add(std::make_shared<quad>(Q, u, v, mat));

    Q = point3(min_corner.x(), min_corner.y(), min_corner.z()); // left face
    u = dz;
    v = dy;
    sides->add(std::make_shared<quad>(Q, u, v, mat));

    Q = point3(min_corner.x(), max_corner.y(), min_corner.z()); // top face
    u = dz;
    v = dx;
    sides->add(std::make_shared<quad>(Q, u, v, mat));
    
    Q = point3(min_corner.x(), min_corner.y(), min_corner.z()); // bottom face
    u = dx;
    v = dz;
    sides->add(std::make_shared<quad>(Q, u, v, mat));

    return sides;
}