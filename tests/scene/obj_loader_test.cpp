#include "pt/geometry/mesh.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include "pt/scene/obj_loader.hpp"
#include "pt/scene/scene_error.hpp"
#include "support/log_silencer.hpp"
#include "support/temp_dir.hpp"
#include "support/test_support.hpp"
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <string_view>

namespace {

using pt::MeshData;
using pt::Point3;
using pt::SceneError;
using pt::load_obj;
using pt::operator""_f;
using pt_test::LogSilencer;
using pt_test::TempDir;
using pt_test::require_uv_near;
using pt_test::require_vec_near;

// A single triangle in the z = 0 plane, wound counter-clockwise seen from +z.
constexpr std::string_view single_triangle = R"(
v 0.0 0.0 0.0
v 1.0 0.0 0.0
v 0.0 1.0 0.0
f 1 2 3
)";

} // namespace

TEST_CASE("a triangle file becomes one triangle with three vertices", "[scene][obj]") {
    LogSilencer silence;
    const TempDir dir;
    const MeshData data = load_obj(dir.write("triangle.obj", single_triangle));

    REQUIRE(data.positions.size() == 3);
    REQUIRE(data.indices.size() == 3);
    REQUIRE(data.normals.empty());
    REQUIRE(data.uvs.empty());

    // Face order is preserved, so the k-th index addresses the k-th vertex of the face.
    require_vec_near(data.positions[data.indices[0]], Point3(0.0_f, 0.0_f, 0.0_f));
    require_vec_near(data.positions[data.indices[1]], Point3(1.0_f, 0.0_f, 0.0_f));
    require_vec_near(data.positions[data.indices[2]], Point3(0.0_f, 1.0_f, 0.0_f));
}

TEST_CASE("corners agreeing on every attribute index collapse into one vertex", "[scene][obj]") {
    LogSilencer silence;
    const TempDir dir;

    // A unit square as two faces sharing the diagonal v1-v3.
    constexpr std::string_view quad = R"(
    v 0.0 0.0 0.0
    v 1.0 0.0 0.0
    v 1.0 1.0 0.0
    v 0.0 1.0 0.0
    f 1 2 3
    f 1 3 4
    )";

    const MeshData data = load_obj(dir.write("quad.obj", quad));

    // Six corners, but only four distinct vertices: this is what indexing buys.
    REQUIRE(data.indices.size() == 6);
    REQUIRE(data.positions.size() == 4);

    // Both faces reach the shared corners through the same index.
    REQUIRE(data.indices[0] == data.indices[3]);
    REQUIRE(data.indices[2] == data.indices[4]);
}

TEST_CASE("corners differing in any attribute index stay separate vertices", "[scene][obj]") {
    LogSilencer silence;
    const TempDir dir;

    // The same square, but the two faces claim different normals. Positions v1
    // and v3 are shared geometrically and must still be split - this is the
    // hard-edge case that forces the OBJ index triple into our single buffer.
    constexpr std::string_view hard_edges = R"(
    v 0.0 0.0 0.0
    v 1.0 0.0 0.0
    v 1.0 1.0 0.0
    v 0.0 1.0 0.0
    vn 0.0 0.0 1.0
    vn 1.0 0.0 0.0
    f 1//1 2//1 3//1
    f 1//2 3//2 4//2
    )";

    const MeshData data = load_obj(dir.write("hard_edges.obj", hard_edges));

    REQUIRE(data.indices.size() == 6);
    REQUIRE(data.positions.size() == 6);
    REQUIRE(data.normals.size() == data.positions.size());

    // Two entries carry the same position but different normals.
    require_vec_near(data.positions[data.indices[0]], data.positions[data.indices[3]]);
    REQUIRE(data.indices[0] != data.indices[3]);
    require_vec_near(data.normals[data.indices[0]], pt::Vec3(0.0_f, 0.0_f, 1.0_f));
    require_vec_near(data.normals[data.indices[3]], pt::Vec3(1.0_f, 0.0_f, 0.0_f));
}

TEST_CASE("a polygon face is triangulated", "[scene][obj]") {
    LogSilencer silence;
    const TempDir dir;

    // One quad face, not two triangles.
    constexpr std::string_view ngon = R"(
    v 0.0 0.0 0.0
    v 1.0 0.0 0.0
    v 1.0 1.0 0.0
    v 0.0 1.0 0.0
    f 1 2 3 4
    )";

    const MeshData data = load_obj(dir.write("ngon.obj", ngon));

    REQUIRE(data.indices.size() == 6);
    REQUIRE(data.indices.size() % 3 == 0);
    REQUIRE(data.positions.size() == 4);
}

TEST_CASE("normals and UVs survive when every corner references them", "[scene][obj]") {
    LogSilencer silence;
    const TempDir dir;

    constexpr std::string_view textured = R"(
    v 0.0 0.0 0.0
    v 1.0 0.0 0.0
    v 0.0 1.0 0.0
    vn 0.0 0.0 1.0
    vt 0.0 0.0
    vt 1.0 0.0
    vt 0.25 0.75
    f 1/1/1 2/2/1 3/3/1
    )";

    const MeshData data = load_obj(dir.write("textured.obj", textured));

    REQUIRE(data.positions.size() == 3);
    REQUIRE(data.normals.size() == 3);
    REQUIRE(data.uvs.size() == 3);

    // All three corners share one normal index, yet each gets its own entry:
    // the buffers are parallel to positions, not to the OBJ attribute pools.
    require_vec_near(data.normals[data.indices[1]], pt::Vec3(0.0_f, 0.0_f, 1.0_f));

    // v is stored exactly as written. Flipping it here would double up with the
    // flip ImageTexture already performs.
    const pt::Uv& uv = data.uvs[data.indices[2]];
    require_uv_near(uv.u, uv.v, 0.25_f, 0.75_f);
}

TEST_CASE("an attribute only some faces reference is dropped entirely", "[scene][obj]") {
    LogSilencer silence;
    const TempDir dir;

    // The second face carries no normal index, so no consistent per-vertex
    // normal buffer exists and the whole attribute goes.
    constexpr std::string_view partial = R"(
    v 0.0 0.0 0.0
    v 1.0 0.0 0.0
    v 1.0 1.0 0.0
    v 0.0 1.0 0.0
    vn 0.0 0.0 1.0
    f 1//1 2//1 3//1
    f 1 3 4
    )";

    const MeshData data = load_obj(dir.write("partial.obj", partial));

    REQUIRE(data.normals.empty());
    REQUIRE(data.indices.size() == 6);

    // With normals dropped, the shared corners merge again.
    REQUIRE(data.positions.size() == 4);
}

TEST_CASE("object groups are merged into a single mesh", "[scene][obj]") {
    LogSilencer silence;
    const TempDir dir;

    constexpr std::string_view groups = R"(
    o first
    v 0.0 0.0 0.0
    v 1.0 0.0 0.0
    v 0.0 1.0 0.0
    f 1 2 3
    o second
    v 0.0 0.0 2.0
    v 1.0 0.0 2.0
    v 0.0 1.0 2.0
    f 4 5 6
    )";

    const MeshData data = load_obj(dir.write("groups.obj", groups));

    // One file yields one mesh: material is a mesh-level property here.
    REQUIRE(data.indices.size() == 6);
    REQUIRE(data.positions.size() == 6);
}

TEST_CASE("negative face indices are resolved relative to the end", "[scene][obj]") {
    LogSilencer silence;
    const TempDir dir;

    constexpr std::string_view relative = R"(
    v 0.0 0.0 0.0
    v 1.0 0.0 0.0
    v 0.0 1.0 0.0
    f -3 -2 -1
    )";

    const MeshData data = load_obj(dir.write("relative.obj", relative));

    REQUIRE(data.positions.size() == 3);
    require_vec_near(data.positions[data.indices[0]], Point3(0.0_f, 0.0_f, 0.0_f));
    require_vec_near(data.positions[data.indices[2]], Point3(0.0_f, 1.0_f, 0.0_f));
}

TEST_CASE("unreadable and empty files are rejected", "[scene][obj]") {
    LogSilencer silence;
    const TempDir dir;

    SECTION("a file that does not exist") {
        REQUIRE_THROWS_AS(load_obj(dir.path() / "missing.obj"), SceneError);
    }
    SECTION("a file with vertices but no faces") {
        constexpr std::string_view faceless = R"(
        v 0.0 0.0 0.0
        v 1.0 0.0 0.0
        v 0.0 1.0 0.0
        )";
        REQUIRE_THROWS_AS(load_obj(dir.write("faceless.obj", faceless)), SceneError);
    }
    SECTION("an empty file") {
        REQUIRE_THROWS_AS(load_obj(dir.write("empty.obj", "")), SceneError);
    }
}

TEST_CASE("the loaded buffers satisfy Mesh's invariants", "[scene][obj]") {
    LogSilencer silence;
    const TempDir dir;
    const std::filesystem::path file = dir.write("triangle.obj", single_triangle);

    // The real contract of this stop: what load_obj returns must be directly
    // constructible as a Mesh, with no fixing up in between.
    REQUIRE_NOTHROW(pt::Mesh(load_obj(file), nullptr));

    const pt::Mesh mesh(load_obj(file), nullptr);
    REQUIRE(mesh.triangle_count() == 1);
}
