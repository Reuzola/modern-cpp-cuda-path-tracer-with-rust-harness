#pragma once
#include "pt/core/hit_record.hpp"
#include "pt/core/hittable.hpp"
#include "pt/geometry/triangle.hpp"
#include "pt/math/aabb.hpp"
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

struct TriangleVertices {
    const Point3& a;
    const Point3& b;
    const Point3& c;
};

// Shared vertex/index storage for a triangle mesh. Not a Hittable: rays
// intersect MeshTriangle handles, which reference this data by index.
class Mesh {
public:
    Mesh(std::vector<Point3> positions, std::vector<std::uint32_t> indices, const Material* mat);

    // Neither copyable nor movable: MeshTriangle stores a Mesh*, so relocating
    // the object would dangle every handle into it.
    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;
    Mesh(Mesh&&) = delete;
    Mesh& operator=(Mesh&&) = delete;

    [[nodiscard]] std::size_t triangle_count() const noexcept { return indices_.size() / 3; }

    [[nodiscard]] TriangleVertices triangle(std::size_t index) const noexcept {
        assert(index < triangle_count());

        const Point3& a = positions_[indices_[3 * index]];
        const Point3& b = positions_[indices_[3 * index + 1]];
        const Point3& c = positions_[indices_[3 * index + 2]];

        return TriangleVertices{.a = a, .b = b, .c = c};
    }

    [[nodiscard]] std::span<const Point3> positions() const noexcept { return positions_; }

    [[nodiscard]] std::span<const std::uint32_t> indices() const noexcept { return indices_; }

    [[nodiscard]] const Material* material() const noexcept { return mat_; }

private:
    std::vector<Point3> positions_;
    std::vector<std::uint32_t> indices_;
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
