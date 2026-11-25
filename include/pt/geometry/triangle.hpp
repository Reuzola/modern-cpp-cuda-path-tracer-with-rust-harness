#pragma once
#include "pt/math/aabb.hpp"
#include "pt/math/interval.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include <type_traits>

namespace pt {

// Pure geometric result of a ray-triangle test. Barycentrics are relative to v0: the weight of v0 is 1 - b1 - b2.
struct TriangleHit {
    Float t{};
    Float b1{};
    Float b2{};
};

// What a triangle's intersection test reads, and all it reads: its corners, copied out of the mesh's
// indexed buffers into a tree's leaf order. Trivially copyable for the reason SphereShape is.
struct TriangleCorners {
    Point3 v0;
    Point3 v1;
    Point3 v2;
};

static_assert(std::is_trivially_copyable_v<TriangleCorners>);

// The hit point rebuilt from the vertices: its rounding then scales with their coordinates, which
// the ulp offset is built for. r.at(t) would carry the error of a long t instead.
// Kept out of intersect_triangle so it can run for the closest hit only.
[[nodiscard]] constexpr Point3 barycentric_point(const Point3& v0, const Point3& v1, const Point3& v2, Float b1, Float b2) noexcept {
    return (1.0_f - b1 - b2) * v0 + b1 * v1 + b2 * v2;
}

// The unit normal of a triangle the test accepted, from the same edges the test formed, so the winding
// decides its side. Kept out of intersect_triangle so it runs for the closest hit only.
[[nodiscard]] Vec3 triangle_normal(const Point3& v0, const Point3& v1, const Point3& v2) noexcept;

[[nodiscard]] bool intersect_triangle(const Ray& r, const Interval& ray_t, const Point3& v0,
                                      const Point3& v1, const Point3& v2, TriangleHit& out) noexcept;

// Component-wise min/max over all three vertices. Combining pairwise boxes instead would leak an
// intermediate box's degenerate-axis padding into the result, making the bounds depend on vertex order.
[[nodiscard]] Aabb triangle_bounds(const Point3& v0, const Point3& v1, const Point3& v2) noexcept;

} // namespace pt
