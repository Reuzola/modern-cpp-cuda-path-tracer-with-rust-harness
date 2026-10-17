#include "pt/geometry/mesh.hpp"
#include "pt/core/hit_record.hpp"
#include "pt/geometry/triangle.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
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

Vec3 Mesh::shading_normal(std::size_t index, Float b1, Float b2, const Vec3& geometric) const noexcept {
    const Float b0 = 1.0_f - b1 - b2;
    if (has_normals()) return interpolated_normal(triangle_normals(index), b0, b1, b2, geometric);
    return geometric;
}

Uv Mesh::texture_uv(std::size_t index, Float b1, Float b2) const noexcept {
    const Float b0 = 1.0_f - b1 - b2;
    if (has_uvs()) return interpolated_uv(triangle_uvs(index), b0, b1, b2);
    return Uv{.u = b1, .v = b2};
}

} // namespace pt
