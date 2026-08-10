#include "pt/geometry/mesh.hpp"
#include "pt/core/hit_record.hpp"
#include "pt/core/hittable.hpp"
#include "pt/core/hittable_list.hpp"
#include "pt/geometry/triangle.hpp"
#include "pt/math/aabb.hpp"
#include "pt/math/interval.hpp"
#include "pt/math/ray.hpp"
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

Mesh::Mesh(std::vector<Point3> positions, std::vector<std::uint32_t> indices, const Material* mat)
    : positions_(std::move(positions)), indices_(std::move(indices)), mat_(mat) {
    // Validated once at construction: an out-of-range index would otherwise
    // become a silent out-of-bounds read on every intersection.
    if (indices_.size() % 3 != 0)
        throw std::invalid_argument(std::format("Index count must be a multiple of 3, but got {}.", indices_.size()));

    const std::size_t size = positions_.size();
    for (const std::uint32_t index : indices_) {
        if (static_cast<std::size_t>(index) >= size)
            throw std::invalid_argument(std::format("Index {} is out of bounds for vertex count {}.", index, size));
    }
}

MeshTriangle::MeshTriangle(const Mesh* mesh, std::uint32_t triangle_index) : mesh_(mesh), triangle_index_(triangle_index) {
    assert(mesh != nullptr);
}

bool MeshTriangle::hit(const Ray& r, const Interval& ray_t, HitRecord& rec, Sampler&) const {
    const auto [a, b, c] = mesh_->triangle(triangle_index_);

    TriangleHit tri_hit;
    if (!intersect_triangle(r, ray_t, a, b, c, tri_hit)) return false;

    rec.t = tri_hit.t;
    rec.p = r.at(tri_hit.t);
    rec.u = tri_hit.b1;
    rec.v = tri_hit.b2;
    rec.mat = mesh_->material();
    rec.set_face_normal(r, tri_hit.normal);

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
