#include "pt/math/transform.hpp"
#include "pt/math/aabb.hpp"
#include "pt/math/constants.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include <cassert>
#include <cmath>

namespace pt {

Transform Transform::translation(const Vec3& offset) noexcept {
    const Affine forward{.linear{}, .translation = offset};
    const Affine inverse{.linear{}, .translation = -offset};
    return Transform(forward, inverse);
}

Transform Transform::scaling(const Vec3& factors) noexcept {
    assert(factors.x() != 0 && factors.y() != 0 && factors.z() != 0);

    const Affine forward{
        .linear = Mat3(Vec3(factors.x(), 0, 0), Vec3(0, factors.y(), 0), Vec3(0, 0, factors.z())),
        .translation{},
    };
    const Affine inverse{
        .linear = Mat3(Vec3(1.0_f / factors.x(), 0, 0), Vec3(0, 1.0_f / factors.y(), 0), Vec3(0, 0, 1.0_f / factors.z())),
        .translation{},
    };
    return Transform(forward, inverse);
}

Transform Transform::rotation_x(Float degrees) noexcept {
    const Float radians = degrees_to_radians(degrees);
    const Float s = std::sin(radians);
    const Float c = std::cos(radians);

    const Affine forward{.linear = Mat3(Vec3(1, 0, 0), Vec3(0, c, -s), Vec3(0, s, c)), .translation{}};
    const Affine inverse{.linear = transpose(forward.linear), .translation{}};
    return Transform(forward, inverse);
}

Transform Transform::rotation_y(Float degrees) noexcept {
    const Float radians = degrees_to_radians(degrees);
    const Float s = std::sin(radians);
    const Float c = std::cos(radians);

    const Affine forward{.linear = Mat3(Vec3(c, 0, s), Vec3(0, 1, 0), Vec3(-s, 0, c)), .translation{}};
    const Affine inverse{.linear = transpose(forward.linear), .translation{}};
    return Transform(forward, inverse);
}

Transform Transform::rotation_z(Float degrees) noexcept {
    const Float radians = degrees_to_radians(degrees);
    const Float s = std::sin(radians);
    const Float c = std::cos(radians);

    const Affine forward{.linear = Mat3(Vec3(c, -s, 0), Vec3(s, c, 0), Vec3(0, 0, 1)), .translation{}};
    const Affine inverse{.linear = transpose(forward.linear), .translation{}};
    return Transform(forward, inverse);
}

Transform operator*(const Transform& a, const Transform& b) noexcept {
    // Reversed on purpose: (AB) inverse is (B inverse)(A inverse).
    return Transform(a.m_ * b.m_, b.inv_ * a.inv_);
}

// Single pass over all eight corners: combining intermediate Aabbs would leak
// their degenerate-axis padding into the result.
Aabb Transform::apply_bounds(const Aabb& box) const noexcept {
    Point3 min_pt(infinity, infinity, infinity);
    Point3 max_pt(-infinity, -infinity, -infinity);

    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 2; j++) {
            for (int k = 0; k < 2; k++) {
                const Float x = i ? box.x.max : box.x.min;
                const Float y = j ? box.y.max : box.y.min;
                const Float z = k ? box.z.max : box.z.min;

                const Point3 applied = apply_point(Point3(x, y, z));

                min_pt = Point3(
                    std::fmin(applied.x(), min_pt.x()),
                    std::fmin(applied.y(), min_pt.y()),
                    std::fmin(applied.z(), min_pt.z()));

                max_pt = Point3(
                    std::fmax(applied.x(), max_pt.x()),
                    std::fmax(applied.y(), max_pt.y()),
                    std::fmax(applied.z(), max_pt.z()));
            }
        }
    }

    return Aabb(min_pt, max_pt);
}

} // namespace pt
