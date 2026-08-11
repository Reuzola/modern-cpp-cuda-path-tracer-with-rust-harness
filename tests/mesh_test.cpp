#include "pt/core/hit_record.hpp"
#include "pt/core/hittable.hpp"
#include "pt/core/hittable_list.hpp"
#include "pt/geometry/mesh.hpp"
#include "pt/geometry/triangle.hpp"
#include "pt/materials/lambertian.hpp"
#include "pt/math/aabb.hpp"
#include "pt/math/color.hpp"
#include "pt/math/constants.hpp"
#include "pt/math/interval.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/sampler.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include "pt/textures/solid_color.hpp"
#include "pt/util/arena.hpp"
#include "test_support.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

using pt::Arena;
using pt::Float;
using pt::HitRecord;
using pt::Hittable;
using pt::HittableList;
using pt::Interval;
using pt::Mesh;
using pt::MeshData;
using pt::MeshTriangle;
using pt::Point3;
using pt::Ray;
using pt::Sampler;
using pt::Triangle;
using pt::unit_vector;
using pt::Uv;
using pt::Vec3;
using pt::operator""_f;
using pt_test::require_uv_near;
using pt_test::require_vec_near;
using pt_test::tolerance;
using pt_test::widen;

using Catch::Matchers::WithinAbs;

// The unit square in the z = 0 plane, as two triangles sharing the diagonal
// v0-v2. Four vertices carry six indices: this is what indexing buys.
[[nodiscard]] std::vector<Point3> quad_positions() {
    return {Point3(0.0_f, 0.0_f, 0.0_f), Point3(1.0_f, 0.0_f, 0.0_f), Point3(1.0_f, 1.0_f, 0.0_f), Point3(0.0_f, 1.0_f, 0.0_f)};
}

[[nodiscard]] std::vector<std::uint32_t> quad_indices() {
    return {0, 1, 2, 0, 2, 3};
}

// Deliberately not all equal to the geometric normal (0, 0, 1): an interpolated
// value can only be told apart from the flat one if the vertices disagree.
[[nodiscard]] std::vector<Vec3> quad_normals() {
    return {Vec3(0.0_f, 0.0_f, 1.0_f), Vec3(1.0_f, 0.0_f, 0.0_f), Vec3(0.0_f, 1.0_f, 0.0_f), Vec3(0.0_f, 0.0_f, 1.0_f)};
}

// Each vertex is given its own (x, y) as texture coordinate, so an interpolated
// UV must come back as the xy position of the hit point - on either triangle.
[[nodiscard]] std::vector<Uv> quad_uvs() {
    return {Uv{.u = 0.0_f, .v = 0.0_f}, Uv{.u = 1.0_f, .v = 0.0_f}, Uv{.u = 1.0_f, .v = 1.0_f}, Uv{.u = 0.0_f, .v = 1.0_f}};
}

// Mesh is neither copyable nor movable, but guaranteed copy elision (C++17)
// still lets a prvalue be returned and initialise the caller's object.
[[nodiscard]] Mesh quad_mesh(const pt::Material* mat) {
    return Mesh(MeshData{.positions = quad_positions(), .indices = quad_indices()}, mat);
}

[[nodiscard]] Mesh quad_mesh_with(std::vector<Vec3> normals, std::vector<Uv> uvs, const pt::Material* mat) {
    return Mesh(MeshData{.positions = quad_positions(), .indices = quad_indices(), .normals = std::move(normals), .uvs = std::move(uvs)}, mat);
}

[[nodiscard]] Ray ray_from_above(Float x, Float y) {
    return Ray(Point3(x, y, 1.0_f), Vec3(0.0_f, 0.0_f, -1.0_f));
}

[[nodiscard]] Ray ray_from_below(Float x, Float y) {
    return Ray(Point3(x, y, -1.0_f), Vec3(0.0_f, 0.0_f, 1.0_f));
}

const Interval visible{0.001_f, pt::infinity};

} // namespace

TEST_CASE("an indexed mesh stores each shared vertex once", "[geometry][mesh]") {
    const Mesh mesh = quad_mesh(nullptr);

    REQUIRE(mesh.triangle_count() == 2);
    REQUIRE(mesh.positions().size() == 4);
    REQUIRE(mesh.indices().size() == 6);
}

TEST_CASE("triangle() addresses the mesh's own storage rather than a copy", "[geometry][mesh]") {
    const Mesh mesh = quad_mesh(nullptr);

    const auto [a0, b0, c0] = mesh.triangle(0);
    const auto [a1, b1, c1] = mesh.triangle(1);

    // Both triangles reference the same two vertex objects, not equal copies.
    REQUIRE(&a0 == &mesh.positions()[0]);
    REQUIRE(&a1 == &mesh.positions()[0]);
    REQUIRE(&c0 == &mesh.positions()[2]);
    REQUIRE(&b1 == &mesh.positions()[2]);

    require_vec_near(b0, Point3(1.0_f, 0.0_f, 0.0_f));
    require_vec_near(c1, Point3(0.0_f, 1.0_f, 0.0_f));
}

TEST_CASE("vertex attributes are optional and travel on the position indices", "[geometry][mesh]") {
    SECTION("a mesh without attributes reports none") {
        const Mesh mesh = quad_mesh(nullptr);

        REQUIRE_FALSE(mesh.has_normals());
        REQUIRE_FALSE(mesh.has_uvs());
        REQUIRE(mesh.normals().empty());
        REQUIRE(mesh.uvs().empty());
    }
    SECTION("attributes can be supplied independently of each other") {
        const Mesh normals_only = quad_mesh_with(quad_normals(), {}, nullptr);
        REQUIRE(normals_only.has_normals());
        REQUIRE_FALSE(normals_only.has_uvs());

        const Mesh uvs_only = quad_mesh_with({}, quad_uvs(), nullptr);
        REQUIRE_FALSE(uvs_only.has_normals());
        REQUIRE(uvs_only.has_uvs());
    }
    SECTION("attribute accessors follow the same indices as triangle()") {
        const Mesh mesh = quad_mesh_with(quad_normals(), quad_uvs(), nullptr);

        // Triangle 1 is (v0, v2, v3), so its second vertex attribute is entry 2.
        const auto [n0, n1, n2] = mesh.triangle_normals(1);
        REQUIRE(&n1 == &mesh.normals()[2]);
        require_vec_near(n0, Vec3(0.0_f, 0.0_f, 1.0_f));
        require_vec_near(n2, Vec3(0.0_f, 0.0_f, 1.0_f));

        const auto [uv0, uv1, uv2] = mesh.triangle_uvs(1);
        REQUIRE(&uv1 == &mesh.uvs()[2]);
        require_uv_near(uv0.u, uv0.v, 0.0_f, 0.0_f);
        require_uv_near(uv2.u, uv2.v, 0.0_f, 1.0_f);
    }
}

TEST_CASE("a mesh rejects buffers it cannot index safely", "[geometry][mesh]") {
    const std::vector<Point3> positions = quad_positions();

    SECTION("an index count that is not a multiple of three") {
        const MeshData data{.positions = positions, .indices = {0, 1}};
        REQUIRE_THROWS_AS(Mesh(data, nullptr), std::invalid_argument);
    }
    SECTION("an index past the end of the vertex buffer") {
        const MeshData data{.positions = positions, .indices = {0, 1, 9}};
        REQUIRE_THROWS_AS(Mesh(data, nullptr), std::invalid_argument);
    }
    SECTION("a normal buffer shorter than the vertex buffer") {
        const MeshData data{.positions = positions, .indices = quad_indices(), .normals = {Vec3(0.0_f, 0.0_f, 1.0_f)}};
        REQUIRE_THROWS_AS(Mesh(data, nullptr), std::invalid_argument);
    }
    SECTION("a UV buffer longer than the vertex buffer") {
        std::vector<Uv> uvs = quad_uvs();
        uvs.push_back(Uv{.u = 0.5_f, .v = 0.5_f});

        const MeshData data{.positions = positions, .indices = quad_indices(), .uvs = std::move(uvs)};
        REQUIRE_THROWS_AS(Mesh(data, nullptr), std::invalid_argument);
    }
    SECTION("empty buffers describe a mesh with no triangles") {
        const Mesh mesh(MeshData{}, nullptr);
        REQUIRE(mesh.triangle_count() == 0);
    }
    SECTION("well-formed buffers are accepted, with or without attributes") {
        const MeshData bare{.positions = positions, .indices = quad_indices()};
        REQUIRE_NOTHROW(Mesh(bare, nullptr));

        const MeshData full{.positions = positions, .indices = quad_indices(), .normals = quad_normals(), .uvs = quad_uvs()};
        REQUIRE_NOTHROW(Mesh(full, nullptr));
    }
}

TEST_CASE("a mesh triangle intersects exactly like a standalone triangle", "[geometry][mesh]") {
    Sampler sampler{0};
    const pt::SolidColor albedo{pt::Color(0.5_f, 0.5_f, 0.5_f)};
    const pt::Lambertian mat{&albedo};

    const Mesh mesh = quad_mesh(&mat);
    const MeshTriangle from_mesh(&mesh, 0);
    const Triangle standalone(mesh.positions()[0], mesh.positions()[1], mesh.positions()[2], &mat);

    const Ray r = ray_from_above(0.75_f, 0.25_f);

    HitRecord mesh_rec;
    HitRecord standalone_rec;
    REQUIRE(from_mesh.hit(r, visible, mesh_rec, sampler));
    REQUIRE(standalone.hit(r, visible, standalone_rec, sampler));

    REQUIRE_THAT(widen(mesh_rec.t), WithinAbs(widen(standalone_rec.t), tolerance));
    REQUIRE_THAT(widen(mesh_rec.u), WithinAbs(widen(standalone_rec.u), tolerance));
    REQUIRE_THAT(widen(mesh_rec.v), WithinAbs(widen(standalone_rec.v), tolerance));
    REQUIRE(mesh_rec.front_face == standalone_rec.front_face);
    require_vec_near(mesh_rec.normal, standalone_rec.normal);
    require_vec_near(mesh_rec.p, standalone_rec.p);

    // The material comes from the mesh, not from the handle.
    REQUIRE(mesh_rec.mat == &mat);
}

TEST_CASE("without attributes a hit reports the flat normal and raw barycentrics", "[geometry][mesh]") {
    Sampler sampler{0};
    const Mesh mesh = quad_mesh(nullptr);
    const MeshTriangle tri(&mesh, 0);

    HitRecord rec;
    REQUIRE(tri.hit(ray_from_above(0.75_f, 0.25_f), visible, rec, sampler));

    require_vec_near(rec.normal, Vec3(0.0_f, 0.0_f, 1.0_f));
    // Weights of v1 and v2 respectively; v0 carries the remaining 0.25.
    require_uv_near(rec.u, rec.v, 0.5_f, 0.25_f);
}

TEST_CASE("vertex normals are interpolated across the triangle", "[geometry][mesh]") {
    Sampler sampler{0};
    const Mesh mesh = quad_mesh_with(quad_normals(), {}, nullptr);
    const MeshTriangle tri(&mesh, 0);

    HitRecord rec;

    SECTION("a hit on a vertex reproduces that vertex's normal exactly") {
        REQUIRE(tri.hit(ray_from_above(1.0_f, 0.0_f), visible, rec, sampler));
        require_vec_near(rec.normal, Vec3(1.0_f, 0.0_f, 0.0_f));
    }
    SECTION("a hit on an edge midpoint blends its two endpoints") {
        // 0.5 * (0, 0, 1) + 0.5 * (1, 0, 0), normalised.
        REQUIRE(tri.hit(ray_from_above(0.5_f, 0.0_f), visible, rec, sampler));
        require_vec_near(rec.normal, unit_vector(Vec3(1.0_f, 0.0_f, 1.0_f)));
    }
    SECTION("an interior hit blends all three vertices") {
        // 0.25 * (0, 0, 1) + 0.5 * (1, 0, 0) + 0.25 * (0, 1, 0), normalised.
        REQUIRE(tri.hit(ray_from_above(0.75_f, 0.25_f), visible, rec, sampler));
        require_vec_near(rec.normal, unit_vector(Vec3(2.0_f, 1.0_f, 1.0_f)));
    }
    SECTION("the result is a unit vector even though the inputs blend to a shorter one") {
        REQUIRE(tri.hit(ray_from_above(0.75_f, 0.25_f), visible, rec, sampler));
        REQUIRE_THAT(widen(rec.normal.length()), WithinAbs(1.0, tolerance));
    }
}

TEST_CASE("orientation follows the geometric normal, not the shading normal", "[geometry][mesh]") {
    Sampler sampler{0};
    HitRecord rec;

    SECTION("a back-face hit flips the interpolated normal") {
        const Mesh mesh = quad_mesh_with(quad_normals(), {}, nullptr);
        const MeshTriangle tri(&mesh, 0);

        REQUIRE(tri.hit(ray_from_below(0.75_f, 0.25_f), visible, rec, sampler));

        REQUIRE_FALSE(rec.front_face);
        require_vec_near(rec.normal, -unit_vector(Vec3(2.0_f, 1.0_f, 1.0_f)));
    }
    SECTION("shading normals pointing away from the ray do not make the hit a back face") {
        // Every vertex normal is inverted, so the shading normal faces along the
        // ray while the geometry still faces it. front_face must follow geometry.
        const std::vector<Vec3> inverted(4, Vec3(0.0_f, 0.0_f, -1.0_f));
        const Mesh mesh = quad_mesh_with(inverted, {}, nullptr);
        const MeshTriangle tri(&mesh, 0);

        REQUIRE(tri.hit(ray_from_above(0.75_f, 0.25_f), visible, rec, sampler));

        REQUIRE(rec.front_face);
        require_vec_near(rec.normal, Vec3(0.0_f, 0.0_f, -1.0_f));
    }
    SECTION("vertex normals that cancel out fall back to the geometric normal") {
        std::vector<Vec3> opposing = quad_normals();
        opposing[0] = Vec3(0.0_f, 0.0_f, 1.0_f);
        opposing[1] = Vec3(0.0_f, 0.0_f, -1.0_f);

        const Mesh mesh = quad_mesh_with(std::move(opposing), {}, nullptr);
        const MeshTriangle tri(&mesh, 0);

        // The edge midpoint weights v0 and v1 equally, summing to the zero vector.
        REQUIRE(tri.hit(ray_from_above(0.5_f, 0.0_f), visible, rec, sampler));

        REQUIRE(rec.front_face);
        require_vec_near(rec.normal, Vec3(0.0_f, 0.0_f, 1.0_f));
    }
}

TEST_CASE("vertex UVs are interpolated across the triangle", "[geometry][mesh]") {
    Sampler sampler{0};
    const Mesh mesh = quad_mesh_with({}, quad_uvs(), nullptr);
    Arena<Hittable> arena;
    const HittableList* triangles = pt::mesh_triangles(arena, mesh);

    HitRecord rec;

    // Each vertex's UV equals its xy position, so an interpolated UV must equal
    // the xy of the hit point - and must agree across the shared diagonal.
    SECTION("on the first triangle") {
        REQUIRE(triangles->hit(ray_from_above(0.75_f, 0.25_f), visible, rec, sampler));
        require_uv_near(rec.u, rec.v, 0.75_f, 0.25_f);
    }
    SECTION("on the second triangle") {
        REQUIRE(triangles->hit(ray_from_above(0.25_f, 0.75_f), visible, rec, sampler));
        require_uv_near(rec.u, rec.v, 0.25_f, 0.75_f);
    }
    SECTION("at a vertex the UV is that vertex's own") {
        REQUIRE(triangles->hit(ray_from_above(1.0_f, 1.0_f), visible, rec, sampler));
        require_uv_near(rec.u, rec.v, 1.0_f, 1.0_f);
    }
    SECTION("UVs do not disturb the geometric normal") {
        REQUIRE(triangles->hit(ray_from_above(0.75_f, 0.25_f), visible, rec, sampler));
        require_vec_near(rec.normal, Vec3(0.0_f, 0.0_f, 1.0_f));
    }
}

TEST_CASE("a mesh triangle's bounding box encloses its three vertices", "[geometry][mesh]") {
    const Mesh mesh = quad_mesh(nullptr);
    const MeshTriangle tri(&mesh, 1); // v0, v2, v3
    const pt::Aabb bbox = tri.bounding_box();

    REQUIRE_THAT(widen(bbox.x.min), WithinAbs(0.0, tolerance));
    REQUIRE_THAT(widen(bbox.x.max), WithinAbs(1.0, tolerance));
    REQUIRE_THAT(widen(bbox.y.min), WithinAbs(0.0, tolerance));
    REQUIRE_THAT(widen(bbox.y.max), WithinAbs(1.0, tolerance));
    REQUIRE(bbox.z.size() > 0.0_f);
}

TEST_CASE("mesh_triangles produces one hittable per triangle", "[geometry][mesh]") {
    const Mesh mesh = quad_mesh(nullptr);
    Arena<Hittable> arena;

    const HittableList* triangles = pt::mesh_triangles(arena, mesh);

    REQUIRE(triangles != nullptr);
    REQUIRE(triangles->objects().size() == mesh.triangle_count());

    const pt::Aabb bbox = triangles->bounding_box();
    REQUIRE_THAT(widen(bbox.x.min), WithinAbs(0.0, tolerance));
    REQUIRE_THAT(widen(bbox.y.max), WithinAbs(1.0, tolerance));
}

TEST_CASE("both halves of a two-triangle quad are reachable through the list", "[geometry][mesh]") {
    Sampler sampler{0};
    const Mesh mesh = quad_mesh(nullptr);
    Arena<Hittable> arena;
    const HittableList* triangles = pt::mesh_triangles(arena, mesh);

    HitRecord rec;

    SECTION("a point below the shared diagonal lands on the first triangle") {
        REQUIRE(triangles->hit(ray_from_above(0.75_f, 0.25_f), visible, rec, sampler));
        require_vec_near(rec.normal, Vec3(0.0_f, 0.0_f, 1.0_f));
    }
    SECTION("a point above the shared diagonal lands on the second triangle") {
        REQUIRE(triangles->hit(ray_from_above(0.25_f, 0.75_f), visible, rec, sampler));
        require_vec_near(rec.normal, Vec3(0.0_f, 0.0_f, 1.0_f));
    }
    SECTION("a point on the shared diagonal is still covered") {
        REQUIRE(triangles->hit(ray_from_above(0.5_f, 0.5_f), visible, rec, sampler));
    }
    SECTION("a point outside the quad misses every triangle") {
        REQUIRE_FALSE(triangles->hit(ray_from_above(1.5_f, 1.5_f), visible, rec, sampler));
    }
}
