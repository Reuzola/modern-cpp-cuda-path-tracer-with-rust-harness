#pragma once
#include "pt/core/hit_record.hpp"
#include "pt/core/hittable.hpp"
#include "pt/geometry/triangle.hpp"
#include "pt/math/aabb.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include "pt/util/arena.hpp"
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>
#include <vector>

namespace pt {

class HittableList;

class Interval;

class Material;

class Ray;

class Sampler;

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

// Shared vertex/index storage for a triangle mesh. Not a Hittable: rays
// intersect MeshTriangle handles, which reference this data by index.
class Mesh {
public:
    Mesh(MeshData data, const Material* mat);

    // Neither copyable nor movable: MeshTriangle stores a Mesh*, so relocating
    // the object would dangle every handle into it.
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

// One triangle of a Mesh, as a (mesh, index) handle. Bounding boxes are
// recomputed rather than cached - they are read at BVH build time only.
class MeshTriangle final : public Hittable {
public:
    MeshTriangle(const Mesh* mesh, std::uint32_t triangle_index);

    [[nodiscard]] bool hit(const Ray& r, const Interval& ray_t, HitRecord& rec, Sampler& sampler) const override;

    [[nodiscard]] Aabb bounding_box() const override;

private:
    const Mesh* mesh_ = nullptr;
    std::uint32_t triangle_index_{};
};

[[nodiscard]] const HittableList* mesh_triangles(Arena<Hittable>& arena, const Mesh& mesh);

} // namespace pt
