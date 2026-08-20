#pragma once
#include "pt/math/interval.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include <algorithm>

namespace pt {

class Aabb {
public:
    Interval x, y, z;

    constexpr Aabb() = default;
    constexpr Aabb(const Interval& x, const Interval& y, const Interval& z) : x(x), y(y), z(z) { pad_to_minimums(); }
    Aabb(const Point3& a, const Point3& b) {
        x = (a[0] <= b[0]) ? Interval(a[0], b[0]) : Interval(b[0], a[0]);
        y = (a[1] <= b[1]) ? Interval(a[1], b[1]) : Interval(b[1], a[1]);
        z = (a[2] <= b[2]) ? Interval(a[2], b[2]) : Interval(b[2], a[2]);
        pad_to_minimums();
    }
    constexpr Aabb(const Aabb& box0, const Aabb& box1) : x(box0.x, box1.x), y(box0.y, box1.y), z(box0.z, box1.z) {}

    [[nodiscard]] constexpr const Interval& axis_interval(int n) const {
        if (n == 1) return y;
        if (n == 2) return z;
        return x;
    }

    [[nodiscard]] constexpr int longest_axis() const {
        if (x.size() > y.size()) return x.size() > z.size() ? 0 : 2;
        return y.size() > z.size() ? 1 : 2;
    }

    // Surface area of the box, the geometric term in the SAH cost function: the probability that a random ray hitting
    // a node also hits a child is the ratio of their areas. Returns 0 for the default-constructed (empty) box.
    [[nodiscard]] constexpr Float surface_area() const {
        const Float dx = std::max(Float{0}, x.size());
        const Float dy = std::max(Float{0}, y.size());
        const Float dz = std::max(Float{0}, z.size());
        return 2.0_f * (dx * dy + dy * dz + dz * dx);
    }

    // Midpoint of the box. Undefined for the empty box: the SAH builder only asks for centroids of real primitive bounds.
    [[nodiscard]] Point3 centroid() const {
        return Point3((x.min + x.max) / 2.0_f, (y.min + y.max) / 2.0_f, (z.min + z.max) / 2.0_f);
    }

    [[nodiscard]] bool hit(const Ray& r, Interval ray_t) const {
        const auto& ray_orig = r.origin();
        const auto& ray_dir = r.direction();

        for (int axis = 0; axis < 3; axis++) {
            const Interval& ax = axis_interval(axis);
            const Float adinv = 1.0_f / ray_dir[axis];

            Float t0 = (ax.min - ray_orig[axis]) * adinv;
            Float t1 = (ax.max - ray_orig[axis]) * adinv;

            if (t0 < t1) {
                if (t0 > ray_t.min) ray_t.min = t0;
                if (t1 < ray_t.max) ray_t.max = t1;
            } else {
                if (t1 > ray_t.min) ray_t.min = t1;
                if (t0 < ray_t.max) ray_t.max = t0;
            }
            if (ray_t.max <= ray_t.min) return false;
        }
        return true;
    }

private:
    constexpr void pad_to_minimums() {
        constexpr Float delta = 0.0001_f;

        if (x.size() < delta) x = x.expand(delta);
        if (y.size() < delta) y = y.expand(delta);
        if (z.size() < delta) z = z.expand(delta);
    }
};

[[nodiscard]] constexpr inline Aabb operator+(const Aabb& box, const Vec3& offset) {
    return Aabb(box.x + offset.x(), box.y + offset.y(), box.z + offset.z());
}

[[nodiscard]] constexpr inline Aabb operator+(const Vec3& offset, const Aabb& box) {
    return box + offset;
}

} // namespace pt
