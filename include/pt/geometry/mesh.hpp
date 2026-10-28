#pragma once
#include "pt/core/hit_record.hpp"
#include "pt/geometry/triangle.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>
#include <vector>

namespace pt {

class Material;

// Texture coordinate for one vertex. Deliberately arithmetic-free: the only
// operation the engine performs on it is barycentric interpolation.
struct Uv {
    Float u{};
    Float v{};
};

struct TriangleVertices {
    const Point3& a;
    const Point3& b;
    const Point3& c;
};

struct TriangleNormals {
    const Vec3& a;
    const Vec3& b;
    const Vec3& c;
};

struct TriangleUvs {
    const Uv& a;
    const Uv& b;
    const Uv& c;
};

// Raw buffers for a triangle mesh. normals and uvs are optional: either empty,
// or exactly as long as positions - they are addressed by the same index
// buffer, so a vertex is one (position, normal, uv) triple.
struct MeshData {
    std::vector<Point3> positions{};
    std::vector<std::uint32_t> indices{};
    std::vector<Vec3> normals{};
    std::vector<Uv> uvs{};
};

// Shared vertex/index storage for a triangle mesh. Not a Hittable: rays intersect the trees whose levels
// hold its triangles, which copy the corners out and come back here by triangle index for the attributes.
class Mesh {
public:
    Mesh(MeshData data, const Material* mat);
    ~Mesh() = default;

    // Neither copyable nor movable: a tree keeps a Mesh* to read attributes on a
    // hit, so relocating the object would dangle every tree that holds its triangles.
    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;
    Mesh(Mesh&&) = delete;
    Mesh& operator=(Mesh&&) = delete;

    [[nodiscard]] std::size_t triangle_count() const noexcept { return data_.indices.size() / 3; }

    [[nodiscard]] bool has_normals() const noexcept { return !data_.normals.empty(); }

    [[nodiscard]] bool has_uvs() const noexcept { return !data_.uvs.empty(); }

    [[nodiscard]] TriangleVertices triangle(std::size_t index) const noexcept {
        assert(index < triangle_count());

        const Point3& a = data_.positions[data_.indices[3 * index]];
        const Point3& b = data_.positions[data_.indices[3 * index + 1]];
        const Point3& c = data_.positions[data_.indices[3 * index + 2]];

        return TriangleVertices{.a = a, .b = b, .c = c};
    }

    [[nodiscard]] TriangleNormals triangle_normals(std::size_t index) const noexcept {
        assert(index < triangle_count());
        assert(has_normals());

        const Vec3& a = data_.normals[data_.indices[3 * index]];
        const Vec3& b = data_.normals[data_.indices[3 * index + 1]];
        const Vec3& c = data_.normals[data_.indices[3 * index + 2]];

        return TriangleNormals{.a = a, .b = b, .c = c};
    }

    [[nodiscard]] TriangleUvs triangle_uvs(std::size_t index) const noexcept {
        assert(index < triangle_count());
        assert(has_uvs());

        const Uv& a = data_.uvs[data_.indices[3 * index]];
        const Uv& b = data_.uvs[data_.indices[3 * index + 1]];
        const Uv& c = data_.uvs[data_.indices[3 * index + 2]];

        return TriangleUvs{.a = a, .b = b, .c = c};
    }

    // The normal shading uses at barycentrics (b1, b2) of triangle `index`: the vertex normals blended, or
    // `geometric` when the mesh has none or the blend cancels out. Read once per hit, never per test.
    [[nodiscard]] Vec3 shading_normal(std::size_t index, Float b1, Float b2, const Vec3& geometric) const noexcept;

    // Texture coordinate at barycentrics (b1, b2) of triangle `index`: the vertex UVs blended, or the
    // barycentrics themselves when the mesh has none, which is what a lone triangle reports.
    [[nodiscard]] Uv texture_uv(std::size_t index, Float b1, Float b2) const noexcept;

    [[nodiscard]] std::span<const Point3> positions() const noexcept { return data_.positions; }

    [[nodiscard]] std::span<const std::uint32_t> indices() const noexcept { return data_.indices; }

    [[nodiscard]] std::span<const Vec3> normals() const noexcept { return data_.normals; }

    [[nodiscard]] std::span<const Uv> uvs() const noexcept { return data_.uvs; }

    [[nodiscard]] const Material* material() const noexcept { return mat_; }

private:
    MeshData data_;
    const Material* mat_ = nullptr;
};

static_assert(!std::is_move_constructible_v<Mesh>);

} // namespace pt
