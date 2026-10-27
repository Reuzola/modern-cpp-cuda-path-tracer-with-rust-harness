#include "pt/geometry/primitive_store.hpp"
#include "pt/core/hit_record.hpp"
#include "pt/geometry/mesh.hpp"
#include "pt/geometry/primitive_kind.hpp"
#include "pt/geometry/quad.hpp"
#include "pt/geometry/sphere.hpp"
#include "pt/geometry/triangle.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/scalar.hpp"
#include <cassert>
#include <cstddef>
#include <cstdint>

namespace pt {

void PrimitiveStore::reserve(std::size_t spheres, std::size_t quads, std::size_t triangles) {
    spheres_.shapes.reserve(spheres);
    spheres_.materials.reserve(spheres);
    quads_.shapes.reserve(quads);
    quads_.materials.reserve(quads);
    triangles_.corners.reserve(triangles);
    triangles_.ids.reserve(triangles);
}

std::uint32_t PrimitiveStore::add_sphere(const Sphere& sphere) {
    const std::uint32_t slot = static_cast<std::uint32_t>(spheres_.shapes.size());
    spheres_.shapes.push_back(sphere.shape());
    spheres_.materials.push_back(sphere.material());
    return slot;
}

std::uint32_t PrimitiveStore::add_quad(const Quad& quad) {
    const std::uint32_t slot = static_cast<std::uint32_t>(quads_.shapes.size());
    quads_.shapes.push_back(quad.shape());
    quads_.materials.push_back(quad.material());
    return slot;
}

std::uint32_t PrimitiveStore::add_triangle(std::uint32_t mesh, std::uint32_t triangle_index) {
    assert(mesh < triangles_.meshes.size());

    const Mesh& source = *triangles_.meshes[mesh];
    const std::uint32_t slot = static_cast<std::uint32_t>(triangles_.corners.size());
    const auto [a, b, c] = source.triangle(triangle_index);
    triangles_.corners.push_back(TriangleCorners{.v0 = a, .v1 = b, .v2 = c});
    triangles_.ids.push_back(triangle_index);

    if (triangles_.meshes.size() > 1) triangles_.mesh_of.push_back(mesh);
    return slot;
}

std::uint32_t PrimitiveStore::add_mesh(const Mesh& mesh) {
    const std::uint32_t index = static_cast<std::uint32_t>(triangles_.meshes.size());
    triangles_.meshes.push_back(&mesh);

    // The second mesh is the first that needs telling apart: every slot so far belongs to the first.
    if (triangles_.meshes.size() == 2) triangles_.mesh_of.assign(triangles_.corners.size(), 0);
    return index;
}

void PrimitiveStore::fill(const PrimitiveHit& hit, const Ray& r, HitRecord& rec) const noexcept {
    assert(hit.kind != PrimitiveKind::opaque);

    switch (hit.kind) {
    case PrimitiveKind::sphere:
        fill_sphere_record(r, spheres_.shapes[hit.sphere.slot], hit.sphere.hit, spheres_.materials[hit.sphere.slot], rec);
        break;
    case PrimitiveKind::quad:
        fill_quad_record(r, quads_.shapes[hit.quad.slot], hit.quad.hit, quads_.materials[hit.quad.slot], rec);
        break;
    case PrimitiveKind::triangle:
        fill_triangle(r, hit.triangle, rec);
        break;
    case PrimitiveKind::opaque:
        // Excluded above: an opaque child fills its own record.
        break;
    }
}

void PrimitiveStore::fill_triangle(const Ray& r, const SlotHit<TriangleHit>& winner, HitRecord& rec) const noexcept {
    const std::uint32_t entry = triangles_.mesh_of.empty() ? 0 : triangles_.mesh_of[winner.slot];
    const Mesh& mesh = *triangles_.meshes[entry];
    const TriangleCorners& tri = triangles_.corners[winner.slot];
    const std::uint32_t id = triangles_.ids[winner.slot];
    const Uv uv = mesh.texture_uv(id, winner.hit.b1, winner.hit.b2);

    rec.t = winner.hit.t;
    rec.p = barycentric_point(tri.v0, tri.v1, tri.v2, winner.hit.b1, winner.hit.b2);
    rec.spawn_margin = 0.0_f; // Written explicitly: the caller reuses records across queries.
    rec.u = uv.u;
    rec.v = uv.v;
    rec.mat = mesh.material();
    rec.set_face_normal(r, winner.hit.normal, mesh.shading_normal(id, winner.hit.b1, winner.hit.b2, winner.hit.normal));
}

} // namespace pt
