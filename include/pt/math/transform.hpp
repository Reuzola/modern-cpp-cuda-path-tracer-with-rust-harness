#pragma once
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include <array>
#include <cassert>
#include <cstddef>
#include <type_traits>

namespace pt {

class Aabb;

class Mat3 {
public:
    Mat3() = default;

    Mat3(const Vec3& row0, const Vec3& row1, const Vec3& row2) : rows_{row0, row1, row2} {}

    [[nodiscard]] const Vec3& row(int i) const noexcept {
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

[[nodiscard]] inline Vec3 operator*(const Mat3& m, const Vec3& v) noexcept {
    return Vec3(dot(m.row(0), v), dot(m.row(1), v), dot(m.row(2), v));
}

[[nodiscard]] inline Mat3 operator*(const Mat3& a, const Mat3& b) noexcept {
    const Vec3 r0 = a.row(0).x() * b.row(0) + a.row(0).y() * b.row(1) + a.row(0).z() * b.row(2);
    const Vec3 r1 = a.row(1).x() * b.row(0) + a.row(1).y() * b.row(1) + a.row(1).z() * b.row(2);
    const Vec3 r2 = a.row(2).x() * b.row(0) + a.row(2).y() * b.row(1) + a.row(2).z() * b.row(2);
    return Mat3(r0, r1, r2);
}

[[nodiscard]] inline Mat3 transpose(const Mat3& m) noexcept {
    const Vec3 r0 = Vec3(m.row(0).x(), m.row(1).x(), m.row(2).x());
    const Vec3 r1 = Vec3(m.row(0).y(), m.row(1).y(), m.row(2).y());
    const Vec3 r2 = Vec3(m.row(0).z(), m.row(1).z(), m.row(2).z());
    return Mat3(r0, r1, r2);
}

struct Affine {
    Mat3 linear{};
    Vec3 translation{};

    [[nodiscard]] Point3 apply_point(const Point3& p) const noexcept {
        return linear * p + translation;
    }

    // No translation: a direction is unaffected by where the origin moves.
    [[nodiscard]] Vec3 apply_vector(const Vec3& v) const noexcept {
        return linear * v;
    }
};

[[nodiscard]] inline Affine operator*(const Affine& a, const Affine& b) noexcept {
    const Mat3 linear = a.linear * b.linear;
    const Point3 translation = a.apply_point(b.translation);
    return Affine{.linear = linear, .translation = translation};
}

// An affine transform and its inverse, stored together: intersection needs the
// inverse on every ray, so it is computed once per instance rather than per hit.
class Transform {
public:
    Transform() = default;

    [[nodiscard]] static Transform translation(const Vec3& offset) noexcept;

    [[nodiscard]] static Transform scaling(const Vec3& factors) noexcept;

    [[nodiscard]] static Transform rotation_x(Float degrees) noexcept;
    [[nodiscard]] static Transform rotation_y(Float degrees) noexcept;
    [[nodiscard]] static Transform rotation_z(Float degrees) noexcept;

    [[nodiscard]] Point3 apply_point(const Point3& p) const noexcept { return m_.apply_point(p); }

    [[nodiscard]] Vec3 apply_vector(const Vec3& v) const noexcept { return m_.apply_vector(v); }

    // Normals transform by the inverse transpose: under non-uniform scale, the
    // linear part alone would no longer keep them perpendicular to the surface.
    [[nodiscard]] Vec3 apply_normal(const Vec3& n) const noexcept { return unit_vector(transpose(inv_.linear) * n); }

    [[nodiscard]] Point3 apply_inverse_point(const Point3& p) const noexcept { return inv_.apply_point(p); }

    [[nodiscard]] Vec3 apply_inverse_vector(const Vec3& v) const noexcept { return inv_.apply_vector(v); }

    [[nodiscard]] Aabb apply_bounds(const Aabb& box) const noexcept;

    friend Transform operator*(const Transform& a, const Transform& b) noexcept;

private:
    Transform(const Affine& m, const Affine& inv) noexcept : m_(m), inv_(inv) {}

    Affine m_{};
    Affine inv_{};
};

static_assert(std::is_trivially_copyable_v<Transform>);

[[nodiscard]] Transform operator*(const Transform& a, const Transform& b) noexcept;

} // namespace pt
