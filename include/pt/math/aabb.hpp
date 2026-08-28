#pragma once
#include "pt/math/interval.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include <algorithm>
#include <optional>
#include <utility>

namespace pt {

class Aabb {
public:
    Interval x, y, z;

    constexpr Aabb() noexcept = default;
    constexpr Aabb(const Interval& x_range, const Interval& y_range, const Interval& z_range) noexcept
        : x(x_range), y(y_range), z(z_range) { pad_to_minimums(); }

    constexpr Aabb(const Point3& a, const Point3& b) noexcept {
        x = (a[0] <= b[0]) ? Interval(a[0], b[0]) : Interval(b[0], a[0]);
        y = (a[1] <= b[1]) ? Interval(a[1], b[1]) : Interval(b[1], a[1]);
        z = (a[2] <= b[2]) ? Interval(a[2], b[2]) : Interval(b[2], a[2]);
        pad_to_minimums();
    }

    constexpr Aabb(const Aabb& box0, const Aabb& box1) noexcept : x(box0.x, box1.x), y(box0.y, box1.y), z(box0.z, box1.z) {}

    [[nodiscard]] constexpr const Interval& axis_interval(int n) const noexcept {
        if (n == 1) return y;
        if (n == 2) return z;
        return x;
    }

    [[nodiscard]] constexpr int longest_axis() const noexcept {
        if (x.size() > y.size()) return x.size() > z.size() ? 0 : 2;
        return y.size() > z.size() ? 1 : 2;
    }

    // Surface area of the box, the geometric term in the SAH cost function: the probability that a random ray hitting
    // a node also hits a child is the ratio of their areas. Returns 0 for the default-constructed (empty) box.
    [[nodiscard]] constexpr Float surface_area() const noexcept {
        const Float dx = std::max(Float{0}, x.size());
        const Float dy = std::max(Float{0}, y.size());
        const Float dz = std::max(Float{0}, z.size());
        return 2.0_f * (dx * dy + dy * dz + dz * dx);
    }

    // Midpoint of the box. Undefined for the empty box: the SAH builder only asks for centroids of real primitive bounds.
    [[nodiscard]] constexpr Point3 centroid() const noexcept {
        return Point3((x.min + x.max) / 2.0_f, (y.min + y.max) / 2.0_f, (z.min + z.max) / 2.0_f);
    }

    // Slab test using precomputed `inv_dir`. Returns the entry distance on hit
    // (clamped to `ray_t.min` if starting inside) for BVH sorting, or nullopt on miss.
    // Conservative by design: edge cases may yield false accepts, never false rejects.
    [[nodiscard]] constexpr std::optional<Float> intersect(const Point3& origin, const Vec3& inv_dir, Interval ray_t) const noexcept {
        for (int axis = 0; axis < 3; axis++) {
            const Interval& ax = axis_interval(axis);
            const Float inv = inv_dir[axis];

            Float t_near = (ax.min - origin[axis]) * inv;
            Float t_far = (ax.max - origin[axis]) * inv;

            // Ordered by the sign of the reciprocal rather than by comparing the two distances. A
            // zero direction component makes one of them NaN, and NaN loses every comparison, so a
            // distance-based test takes the wrong branch and rejects a box the ray actually touches.
            // The sign is never NaN, so ordering by it leaves any NaN in t_far, where
            // `t_far < ray_t.max` is false and the bound is simply left alone: the box stays
            // conservatively accepted and the primitive test decides.
            if (inv < 0) std::swap(t_near, t_far);
            if (t_near > ray_t.min) ray_t.min = t_near;
            if (t_far < ray_t.max) ray_t.max = t_far;

            if (ray_t.max <= ray_t.min) return std::nullopt;
        }
        return ray_t.min;
    }

private:
    constexpr void pad_to_minimums() noexcept {
        constexpr Float delta = 0.0001_f;

        if (x.size() < delta) x = x.expand(delta);
        if (y.size() < delta) y = y.expand(delta);
        if (z.size() < delta) z = z.expand(delta);
    }
};

[[nodiscard]] constexpr Aabb operator+(const Aabb& box, const Vec3& offset) noexcept {
    return Aabb(box.x + offset.x(), box.y + offset.y(), box.z + offset.z());
}

[[nodiscard]] constexpr Aabb operator+(const Vec3& offset, const Aabb& box) noexcept {
    return box + offset;
}

} // namespace pt
