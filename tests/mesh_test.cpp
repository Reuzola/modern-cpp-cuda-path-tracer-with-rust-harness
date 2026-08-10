#include "pt/geometry/mesh.hpp"
#include "pt/core/hit_record.hpp"
#include "pt/core/hittable.hpp"
#include "pt/core/hittable_list.hpp"
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
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace {

using pt::Arena;
using pt::Float;
using pt::HitRecord;
using pt::Hittable;
using pt::HittableList;
using pt::Interval;
using pt::Mesh;
using pt::MeshTriangle;
using pt::Point3;
using pt::Ray;
using pt::Sampler;
using pt::Triangle;
using pt::Vec3;
using pt::operator""_f;

using Catch::Matchers::WithinAbs;

[[nodiscard]] double widen(Float v) noexcept { return static_cast<double>(v); }

constexpr double tolerance = 1e-6;

void require_vec_near(const Vec3& actual, const Vec3& expected) {
    REQUIRE_THAT(widen(actual.x()), WithinAbs(widen(expected.x()), tolerance));
    REQUIRE_THAT(widen(actual.y()), WithinAbs(widen(expected.y()), tolerance));
    REQUIRE_THAT(widen(actual.z()), WithinAbs(widen(expected.z()), tolerance));
}

// The unit square in the z = 0 plane, as two triangles sharing the diagonal
// v0-v2. Four vertices carry six indices: this is what indexing buys.
[[nodiscard]] std::vector<Point3> quad_positions() {
    return {Point3(0.0_f, 0.0_f, 0.0_f), Point3(1.0_f, 0.0_f, 0.0_f), Point3(1.0_f, 1.0_f, 0.0_f), Point3(0.0_f, 1.0_f, 0.0_f)};
}

[[nodiscard]] std::vector<std::uint32_t> quad_indices() {
    return {0, 1, 2, 0, 2, 3};
}

// Mesh is neither copyable nor movable, but guaranteed copy elision (C++17)
// still lets a prvalue be returned and initialise the caller's object.
[[nodiscard]] Mesh quad_mesh(const pt::Material* mat) {
    return Mesh(quad_positions(), quad_indices(), mat);
}

[[nodiscard]] Ray ray_from_above(Float x, Float y) {
    return Ray(Point3(x, y, 1.0_f), Vec3(0.0_f, 0.0_f, -1.0_f));
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

TEST_CASE("a mesh rejects buffers it cannot index safely", "[geometry][mesh]") {
    const std::vector<Point3> positions = quad_positions();

    SECTION("an index count that is not a multiple of three") {
        const std::vector<std::uint32_t> indices{0, 1};
        REQUIRE_THROWS_AS(Mesh(positions, indices, nullptr), std::invalid_argument);
    }
    SECTION("an index past the end of the vertex buffer") {
        const std::vector<std::uint32_t> indices{0, 1, 9};
        REQUIRE_THROWS_AS(Mesh(positions, indices, nullptr), std::invalid_argument);
    }
    SECTION("empty buffers describe a mesh with no triangles") {
        const Mesh mesh({}, {}, nullptr);
        REQUIRE(mesh.triangle_count() == 0);
    }
    SECTION("well-formed buffers are accepted") {
        REQUIRE_NOTHROW(Mesh(positions, quad_indices(), nullptr));
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
