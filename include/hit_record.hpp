#pragma once
#include "ray.hpp"
#include "vec3.hpp"

class material;

struct hit_record {
    point3 p;
    vec3 normal;
    double t{};
    double u{};
    double v{};
    bool front_face{};
    const material* mat = nullptr;

    void set_face_normal(const ray& r, const vec3& outward_normal) {
        front_face = dot(r.direction(), outward_normal) < 0;
        normal = front_face ? outward_normal : -outward_normal;
    }
};