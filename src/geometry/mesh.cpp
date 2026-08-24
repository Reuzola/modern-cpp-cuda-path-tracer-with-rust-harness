#include "pt/geometry/mesh.hpp"
#include "pt/core/hit_record.hpp"
#include "pt/core/hittable.hpp"
#include "pt/core/hittable_list.hpp"
#include "pt/geometry/triangle.hpp"
#include "pt/math/aabb.hpp"
#include "pt/math/interval.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include "pt/util/arena.hpp"
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <format>
#include <stdexcept>
#include <utility>
#include <vector>

namespace pt {

namespace {

[[nodiscard]] Vec3 interpolated_normal(const TriangleNormals& n, Float b0, Float b1, Float b2, const Vec3& fallback) noexcept {
    const Vec3 total = b0 * n.a + b1 * n.b + b2 * n.c;

    // Falls back to the geometric normal when the interpolated one collapses:
    // opposing vertex normals would otherwise normalise to NaN.
    if (total.near_zero()) return fallback;
    return unit_vector(total);
}

[[nodiscard]] Uv interpolated_uv(const TriangleUvs& uv, Float b0, Float b1, Float b2) noexcept {
    const Float u = b0 * uv.a.u + b1 * uv.b.u + b2 * uv.c.u;
    const Float v = b0 * uv.a.v + b1 * uv.b.v + b2 * uv.c.v;

    return Uv{.u = u, .v = v};
}

} // namespace

Mesh::Mesh(MeshData data, const Material* mat) : data_(std::move(data)), mat_(mat) {
    // Validated once at construction: an out-of-range index would otherwise
    // become a silent out-of-bounds read on every intersection.
    if (data_.indices.size() % 3 != 0)
        throw std::invalid_argument(std::format("Index count must be a multiple of 3, but got {}.", data_.indices.size()));

    const std::size_t size = data_.positions.size();

    // Optional attributes share the position index buffer, so a non-empty
    // attribute array must be exactly as long as positions.
    if (!data_.normals.empty() && data_.normals.size() != size)
        throw std::invalid_argument(std::format("Normal count must match vertex count ({}), but got {}.", size, data_.normals.size()));

    if (!data_.uvs.empty() && data_.uvs.size() != size)
        throw std::invalid_argument(std::format("UV count must match vertex count ({}), but got {}.", size, data_.uvs.size()));

    for (const std::uint32_t index : data_.indices) {
        if (static_cast<std::size_t>(index) >= size)
            throw std::invalid_argument(std::format("Index {} is out of bounds for vertex count {}.", index, size));
    }
}

MeshTriangle::MeshTriangle(const Mesh* mesh, std::uint32_t triangle_index) : mesh_(mesh), triangle_index_(triangle_index) {
    assert(mesh != nullptr);
}

bool MeshTriangle::hit(const Ray& r, const Interval& ray_t, HitRecord& rec) const {
    const auto [a, b, c] = mesh_->triangle(triangle_index_);

    TriangleHit tri_hit;
    if (!intersect_triangle(r, ray_t, a, b, c, tri_hit)) return false;

    const Float b0 = 1.0_f - tri_hit.b1 - tri_hit.b2;
    const Vec3 shading_normal = mesh_->has_normals()
                                    ? interpolated_normal(mesh_->triangle_normals(triangle_index_), b0, tri_hit.b1, tri_hit.b2, tri_hit.normal)
                                    : tri_hit.normal;

    const Uv uv = mesh_->has_uvs()
                      ? interpolated_uv(mesh_->triangle_uvs(triangle_index_), b0, tri_hit.b1, tri_hit.b2)
                      : Uv{.u = tri_hit.b1, .v = tri_hit.b2};

    rec.t = tri_hit.t;
    rec.p = r.at(tri_hit.t);
    rec.u = uv.u;
    rec.v = uv.v;
    rec.mat = mesh_->material();
    rec.set_face_normal(r, tri_hit.normal, shading_normal);

    return true;
}

Aabb MeshTriangle::bounding_box() const {
    const auto [a, b, c] = mesh_->triangle(triangle_index_);
    return triangle_bounds(a, b, c);
}

const HittableList* mesh_triangles(Arena<Hittable>& arena, const Mesh& mesh) {
    // Mirrors box(): fills the arena, returns a non-owning view. Wrapping the result in a BVH is the caller's decision.
    HittableList triangles;

    const std::size_t count = mesh.triangle_count();
    for (std::size_t i = 0; i < count; i++) {
        triangles.add(arena.create<MeshTriangle>(&mesh, static_cast<std::uint32_t>(i)));
    }

    return arena.create<HittableList>(std::move(triangles));
}

} // namespace pt
