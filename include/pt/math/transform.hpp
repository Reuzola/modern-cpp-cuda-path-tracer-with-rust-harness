#pragma once
#include "pt/math/aabb.hpp"
#include "pt/math/constants.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <type_traits>

namespace pt {

class Mat3 {
public:
    constexpr Mat3() noexcept = default;

    constexpr Mat3(const Vec3& row0, const Vec3& row1, const Vec3& row2) noexcept : rows_{row0, row1, row2} {}

    [[nodiscard]] constexpr const Vec3& row(int i) const noexcept {
        assert(i >= 0 && i < 3);
        return rows_[static_cast<std::size_t>(i)];
    }

private:
    std::array<Vec3, 3> rows_{
        Vec3(1, 0, 0),
        Vec3(0, 1, 0),
        Vec3(0, 0, 1),
    };
};

[[nodiscard]] constexpr Vec3 operator*(const Mat3& m, const Vec3& v) noexcept {
    return Vec3(dot(m.row(0), v), dot(m.row(1), v), dot(m.row(2), v));
}

[[nodiscard]] constexpr Mat3 operator*(const Mat3& a, const Mat3& b) noexcept {
    const Vec3 r0 = a.row(0).x() * b.row(0) + a.row(0).y() * b.row(1) + a.row(0).z() * b.row(2);
    const Vec3 r1 = a.row(1).x() * b.row(0) + a.row(1).y() * b.row(1) + a.row(1).z() * b.row(2);
    const Vec3 r2 = a.row(2).x() * b.row(0) + a.row(2).y() * b.row(1) + a.row(2).z() * b.row(2);
    return Mat3(r0, r1, r2);
}

[[nodiscard]] constexpr Mat3 transpose(const Mat3& m) noexcept {
    const Vec3 r0 = Vec3(m.row(0).x(), m.row(1).x(), m.row(2).x());
    const Vec3 r1 = Vec3(m.row(0).y(), m.row(1).y(), m.row(2).y());
    const Vec3 r2 = Vec3(m.row(0).z(), m.row(1).z(), m.row(2).z());
    return Mat3(r0, r1, r2);
}

struct Affine {
    Mat3 linear{};
    Vec3 translation{};

    [[nodiscard]] constexpr Point3 apply_point(const Point3& p) const noexcept {
        return linear * p + translation;
    }

    // No translation: a direction is unaffected by where the origin moves.
    [[nodiscard]] constexpr Vec3 apply_vector(const Vec3& v) const noexcept {
        return linear * v;
    }
};

[[nodiscard]] constexpr Affine operator*(const Affine& a, const Affine& b) noexcept {
    const Mat3 linear = a.linear * b.linear;
    const Point3 translation = a.apply_point(b.translation);
    return Affine{.linear = linear, .translation = translation};
}

// An affine transform and its inverse, stored together: intersection needs the
// inverse on every ray, so it is computed once per instance rather than per hit.
class Transform {
public:
    constexpr Transform() noexcept = default;

    [[nodiscard]] static constexpr Transform translation(const Vec3& offset) noexcept {
        const Affine forward{.linear{}, .translation = offset};
        const Affine inverse{.linear{}, .translation = -offset};
        return Transform(forward, inverse);
    }

    [[nodiscard]] static constexpr Transform scaling(const Vec3& factors) noexcept {
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

    [[nodiscard]] static Transform rotation_x(Float degrees) noexcept {
        const Float radians = degrees_to_radians(degrees);
        const Float s = std::sin(radians);
        const Float c = std::cos(radians);

        const Affine forward{.linear = Mat3(Vec3(1, 0, 0), Vec3(0, c, -s), Vec3(0, s, c)), .translation{}};
        const Affine inverse{.linear = transpose(forward.linear), .translation{}};
        return Transform(forward, inverse);
    }

    [[nodiscard]] static Transform rotation_y(Float degrees) noexcept {
        const Float radians = degrees_to_radians(degrees);
        const Float s = std::sin(radians);
        const Float c = std::cos(radians);

        const Affine forward{.linear = Mat3(Vec3(c, 0, s), Vec3(0, 1, 0), Vec3(-s, 0, c)), .translation{}};
        const Affine inverse{.linear = transpose(forward.linear), .translation{}};
        return Transform(forward, inverse);
    }

    [[nodiscard]] static Transform rotation_z(Float degrees) noexcept {
        const Float radians = degrees_to_radians(degrees);
        const Float s = std::sin(radians);
        const Float c = std::cos(radians);

        const Affine forward{.linear = Mat3(Vec3(c, -s, 0), Vec3(s, c, 0), Vec3(0, 0, 1)), .translation{}};
        const Affine inverse{.linear = transpose(forward.linear), .translation{}};
        return Transform(forward, inverse);
    }

    [[nodiscard]] constexpr Point3 apply_point(const Point3& p) const noexcept { return m_.apply_point(p); }

    [[nodiscard]] constexpr Vec3 apply_vector(const Vec3& v) const noexcept { return m_.apply_vector(v); }

    // Normals transform by the inverse transpose: under non-uniform scale, the
    // linear part alone would no longer keep them perpendicular to the surface.
    [[nodiscard]] Vec3 apply_normal(const Vec3& n) const noexcept { return unit_vector(transpose(inv_.linear) * n); }

    [[nodiscard]] constexpr Point3 apply_inverse_point(const Point3& p) const noexcept { return inv_.apply_point(p); }

    [[nodiscard]] constexpr Vec3 apply_inverse_vector(const Vec3& v) const noexcept { return inv_.apply_vector(v); }

    // Single pass over all eight corners: combining intermediate Aabbs would leak
    // their degenerate-axis padding into the result.
    [[nodiscard]] Aabb apply_bounds(const Aabb& box) const noexcept {
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

    friend constexpr Transform operator*(const Transform& a, const Transform& b) noexcept;

private:
    constexpr Transform(const Affine& m, const Affine& inv) noexcept : m_(m), inv_(inv) {}

    Affine m_{};
    Affine inv_{};
};

static_assert(std::is_trivially_copyable_v<Transform>);

[[nodiscard]] constexpr Transform operator*(const Transform& a, const Transform& b) noexcept {
    // Reversed on purpose: (AB) inverse is (B inverse)(A inverse).
    return Transform(a.m_ * b.m_, b.inv_ * a.inv_);
}

} // namespace pt
