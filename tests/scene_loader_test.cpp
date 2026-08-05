#include "pt/core/hittable.hpp"
#include "pt/math/aabb.hpp"
#include "pt/math/color.hpp"
#include "pt/math/interval.hpp"
#include "pt/math/vec3.hpp"
#include "pt/post/tonemap.hpp"
#include "pt/sampling/importance_targets.hpp"
#include "pt/scene/scene.hpp"
#include "pt/scene/scene_loader.hpp"
#include "pt/util/log.hpp"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <string_view>
#include <system_error>

namespace {

// PT_SCENES_DIR is injected by tests/CMakeLists.txt; it appears exactly once.
const std::filesystem::path scenes_dir{PT_SCENES_DIR};

constexpr std::string_view minimal_scene = R"json({
  "version": 1,
  "camera": { "lookfrom": [0, 0, 1], "lookat": [0, 0, 0], "vfov": 90 },
  "render": {
    "width": 4,
    "height": 3,
    "samples_per_pixel": 1,
    "max_depth": 1,
    "background": [0, 0, 0]
  },
  "textures": {
    "grey": { "type": "solid_color", "albedo": [0.5, 0.5, 0.5] }
  },
  "materials": {
    "grey": { "type": "lambertian", "texture": "grey" }
  },
  "objects": [
    { "type": "sphere", "center": [0, 0, 0], "radius": 1, "material": "grey" }
  ]
})json";

// Deliberately non-deterministic, unlike the engine's Sampler: two `ctest -j`
// worker processes must not agree on a fixture directory name.
[[nodiscard]] const std::string& process_token() {
    static const std::string token = [] {
        std::random_device device;
        return std::to_string(device());
    }();
    return token;
}

/// Writes JSON to `<temp>/pt_scene_<token>_<n>/scene.json` and removes the
/// directory on destruction. The scene gets its own directory because the
/// loader resolves `image.filename` relative to the scene file's parent.
class TempSceneDir {
public:
    explicit TempSceneDir(std::string_view json_text) {
        static int counter = 0;
        ++counter;

        dir_ = std::filesystem::temp_directory_path() / ("pt_scene_" + process_token() + "_" + std::to_string(counter));
        path_ = dir_ / "scene.json";

        std::error_code ec;
        std::filesystem::create_directories(dir_, ec);
        REQUIRE(!ec);

        // Binary mode: no newline translation, so the bytes on disk are the
        // bytes in the literal above. ofstream::binary rather than ios::binary
        // so <fstream> is the only header this line needs.
        std::ofstream stream(path_, std::ofstream::binary);
        stream << json_text;
        stream.close();
        REQUIRE(stream.good());
    }

    TempSceneDir(const TempSceneDir&) = delete;
    TempSceneDir& operator=(const TempSceneDir&) = delete;

    // error_code overload, not the throwing one: this destructor runs during
    // unwinding from a failed expectation, where a second exception terminates.
    ~TempSceneDir() {
        std::error_code ec;
        std::filesystem::remove_all(dir_, ec);
    }

    [[nodiscard]] const std::filesystem::path& scene_path() const noexcept { return path_; }

private:
    std::filesystem::path dir_;
    std::filesystem::path path_;
};

/// Suppresses log output for the duration of a scope. Used where a scene is
/// expected to warn - a missing image file is a warning, not a load error.
class LogSilencer {
public:
    LogSilencer() : previous_(pt::log_level()) { pt::set_log_level(pt::LogLevel::off); }

    LogSilencer(const LogSilencer&) = delete;
    LogSilencer& operator=(const LogSilencer&) = delete;

    ~LogSilencer() { pt::set_log_level(previous_); }

private:
    pt::LogLevel previous_;
};

// `margin` exists for boxes: box() emits six flat quads, and Aabb inflates a
// zero-thickness axis by 0.0001 so that rays can enter it at all.
void check_interval(const pt::Interval& interval, double low, double high, double margin = 0.0) {
    CHECK(static_cast<double>(interval.min) == Catch::Approx(low).margin(margin));
    CHECK(static_cast<double>(interval.max) == Catch::Approx(high).margin(margin));
}

void check_vec3(const pt::Vec3& v, double x, double y, double z) {
    CHECK(static_cast<double>(v.x()) == Catch::Approx(x));
    CHECK(static_cast<double>(v.y()) == Catch::Approx(y));
    CHECK(static_cast<double>(v.z()) == Catch::Approx(z));
}

void check_color(const pt::Color& c, double r, double g, double b) {
    CHECK(static_cast<double>(c.r()) == Catch::Approx(r));
    CHECK(static_cast<double>(c.g()) == Catch::Approx(g));
    CHECK(static_cast<double>(c.b()) == Catch::Approx(b));
}

// Every optional field omitted: the defaults table in docs/scene-format.md is
// what this scene asserts. A dielectric material is used so that the whole
// `textures` block can be absent too.
constexpr std::string_view defaults_scene = R"json({
  "version": 1,
  "camera": { "lookfrom": [0, 0, 5], "lookat": [0, 0, 0], "vfov": 40 },
  "render": {
    "width": 2,
    "height": 2,
    "samples_per_pixel": 1,
    "max_depth": 1,
    "background": [0, 0, 0]
  },
  "materials": {
    "glass": { "type": "dielectric", "refraction_index": 1.5 }
  },
  "objects": [
    { "type": "sphere", "center": [0, 0, 0], "radius": 2, "material": "glass" }
  ]
})json";

// The mirror image of defaults_scene: every optional field present, and one
// instance of each texture and material type that needs no external file.
constexpr std::string_view explicit_scene = R"json({
  "version": 1,
  "camera": {
    "lookfrom": [1, 2, 3],
    "lookat": [4, 5, 6],
    "vup": [0, 0, 1],
    "vfov": 35,
    "defocus_angle": 2,
    "focus_dist": 7
  },
  "render": {
    "width": 320,
    "height": 180,
    "samples_per_pixel": 8,
    "max_depth": 12,
    "background": [0.25, 0.5, 0.75],
    "seed": 12345,
    "tone_map": { "exposure": 2.5, "operator": "aces" }
  },
  "textures": {
    "checks": {
      "type": "checker",
      "scale": 0.5,
      "even": { "type": "solid_color", "albedo": [1, 1, 1] },
      "odd": { "type": "solid_color", "albedo": [0, 0, 0] }
    },
    "grainy": { "type": "noise", "scale": 4 }
  },
  "materials": {
    "floor": { "type": "lambertian", "texture": "checks" },
    "rock": { "type": "lambertian", "texture": "grainy" },
    "shiny": { "type": "metal", "albedo": [0.8, 0.8, 0.8], "fuzz": 0.3 },
    "lamp": { "type": "diffuse_light", "texture": "checks" }
  },
  "objects": [
    { "type": "sphere", "center": [0, 0, 0], "radius": 1, "material": "floor" },
    { "type": "quad", "name": "panel", "q": [-1, 2, -1], "u": [2, 0, 0], "v": [0, 0, 2], "material": "lamp" }
  ],
  "importance_targets": ["panel"]
})json";

// A named object referenced from a later slot: the shell is both a visible
// surface and the boundary of the medium, and must remain a single object.
constexpr std::string_view reference_scene = R"json({
  "version": 1,
  "camera": { "lookfrom": [0, 0, 5], "lookat": [0, 0, 0], "vfov": 40 },
  "render": {
    "width": 2,
    "height": 2,
    "samples_per_pixel": 1,
    "max_depth": 1,
    "background": [0, 0, 0]
  },
  "textures": {
    "smoke": { "type": "solid_color", "albedo": [0.2, 0.2, 0.2] }
  },
  "materials": {
    "glass": { "type": "dielectric", "refraction_index": 1.5 },
    "fog": { "type": "isotropic", "texture": "smoke" }
  },
  "objects": [
    { "type": "sphere", "name": "shell", "center": [0, 0, 0], "radius": 1, "material": "glass" },
    { "type": "constant_medium", "boundary": "shell", "density": 0.5, "phase_function": "fog" }
  ]
})json";

// group / translate / rotate_y / box, plus a group child given by name.
constexpr std::string_view composite_scene = R"json({
  "version": 1,
  "camera": { "lookfrom": [0, 0, 5], "lookat": [0, 0, 0], "vfov": 40 },
  "render": {
    "width": 2,
    "height": 2,
    "samples_per_pixel": 1,
    "max_depth": 1,
    "background": [0, 0, 0]
  },
  "textures": {
    "white": { "type": "solid_color", "albedo": [0.73, 0.73, 0.73] }
  },
  "materials": {
    "white": { "type": "lambertian", "texture": "white" }
  },
  "objects": [
    { "type": "sphere", "name": "marker", "center": [10, 0, 0], "radius": 1, "material": "white" },
    {
      "type": "translate",
      "offset": [0, 0, 0],
      "object": {
        "type": "rotate_y",
        "angle": 0,
        "object": {
          "type": "group",
          "children": [
            { "type": "box", "a": [0, 0, 0], "b": [2, 2, 2], "material": "white" },
            "marker"
          ]
        }
      }
    }
  ]
})json";

// A sphere sweeping from center to center_end: its bounding box must cover
// both endpoints, which is the observable trace of motion blur.
constexpr std::string_view motion_scene = R"json({
  "version": 1,
  "camera": { "lookfrom": [0, 0, 5], "lookat": [0, 0, 0], "vfov": 40 },
  "render": {
    "width": 2,
    "height": 2,
    "samples_per_pixel": 1,
    "max_depth": 1,
    "background": [0, 0, 0]
  },
  "materials": {
    "glass": { "type": "dielectric", "refraction_index": 1.5 }
  },
  "objects": [
    {
      "type": "sphere",
      "center": [0, 0, 0],
      "center_end": [4, 0, 0],
      "radius": 1,
      "material": "glass"
    }
  ]
})json";

// The image file is never created next to this scene: an unopenable texture is
// documented as a warning, not a load failure.
constexpr std::string_view missing_image_scene = R"json({
  "version": 1,
  "camera": { "lookfrom": [0, 0, 5], "lookat": [0, 0, 0], "vfov": 40 },
  "render": {
    "width": 2,
    "height": 2,
    "samples_per_pixel": 1,
    "max_depth": 1,
    "background": [0, 0, 0]
  },
  "textures": {
    "absent": { "type": "image", "filename": "no_such_image.jpg" }
  },
  "materials": {
    "surface": { "type": "lambertian", "texture": "absent" }
  },
  "objects": [
    { "type": "sphere", "center": [0, 0, 0], "radius": 1, "material": "surface" }
  ]
})json";

/// A valid scene, with any section replaceable. An empty section is an omitted
/// key, which is how the "missing required field" cases are written.
struct SceneParts {
    std::string_view version = "1";
    std::string_view camera = R"({"lookfrom": [0, 0, 5], "lookat": [0, 0, 0], "vfov": 40})";
    std::string_view render = R"({"width": 2, "height": 2, "samples_per_pixel": 1, "max_depth": 1, "background": [0, 0, 0]})";
    std::string_view textures = R"({"grey": {"type": "solid_color", "albedo": [0.5, 0.5, 0.5]}})";
    std::string_view materials = R"({"grey": {"type": "lambertian", "texture": "grey"}})";
    std::string_view objects = R"([{"type": "sphere", "center": [0, 0, 0], "radius": 1, "material": "grey"}])";
    std::string_view importance_targets = "";
};

[[nodiscard]] std::string compose(const SceneParts& parts) {
    std::string json = "{";

    const auto append = [&json](std::string_view key, std::string_view value) {
        if (value.empty()) return;
        if (json.size() > 1) json += ",";
        json += "\"";
        json += key;
        json += "\":";
        json += value;
    };

    append("version", parts.version);
    append("camera", parts.camera);
    append("render", parts.render);
    append("textures", parts.textures);
    append("materials", parts.materials);
    append("objects", parts.objects);
    append("importance_targets", parts.importance_targets);

    json += "}";
    return json;
}

/// Asserts that loading fails with the exact location path and a message
/// containing `expected_message`. The location is the strict half: it is what
/// tells the user which field is wrong, and it regresses silently.
void check_load_error(const std::filesystem::path& path, std::string_view expected_location, std::string_view expected_message) {
    CAPTURE(expected_location, expected_message);

    try {
        static_cast<void>(pt::load_scene(path));
        FAIL("load_scene accepted a document it should have rejected");
    } catch (const pt::SceneError& e) {
        CHECK(e.location() == expected_location);
        CHECK_THAT(std::string(e.what()), Catch::Matchers::ContainsSubstring(std::string(expected_message)));
    }
}

void check_scene_error(std::string_view json_text, std::string_view expected_location, std::string_view expected_message) {
    const TempSceneDir fixture(json_text);
    check_load_error(fixture.scene_path(), expected_location, expected_message);
}

} // namespace

TEST_CASE("the repository's scene directory is reachable from the test binary", "[scene][loader]") {
    const pt::Scene scene = pt::load_scene(scenes_dir / "cornell_box.json");

    CHECK(scene.render.image_width == 600);
    CHECK(scene.render.image_height == 600);
}

TEST_CASE("a scene written to a temporary directory loads through the same path", "[scene][loader]") {
    const TempSceneDir fixture(minimal_scene);

    const pt::Scene scene = pt::load_scene(fixture.scene_path());

    CHECK(scene.render.image_width == 4);
    CHECK(scene.render.image_height == 3);
    CHECK(scene.importance_targets().empty());
}

TEST_CASE("every scene shipped in the repository loads", "[scene][loader]") {
    // A filtered checkout may lack earth.json's image asset, which warns.
    const LogSilencer silence;

    int loaded = 0;
    for (const auto& entry : std::filesystem::directory_iterator(scenes_dir)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".json") continue;

        CAPTURE(entry.path().filename().string());
        const pt::Scene scene = pt::load_scene(entry.path());

        CHECK(scene.render.image_width > 0);
        CHECK(scene.render.image_height > 0);
        CHECK(scene.render.samples_per_pixel > 0);
        ++loaded;
    }

    // Guards against the failure mode where a wrong PT_SCENES_DIR turns the
    // loop into a no-op and the test passes vacuously.
    REQUIRE(loaded > 0);
}

TEST_CASE("omitted optional fields take the documented defaults", "[scene][loader]") {
    const TempSceneDir fixture(defaults_scene);
    const pt::Scene scene = pt::load_scene(fixture.scene_path());

    check_vec3(scene.camera.vup, 0, 1, 0);
    CHECK(static_cast<double>(scene.camera.defocus_angle) == Catch::Approx(0.0));
    CHECK(static_cast<double>(scene.camera.focus_dist) == Catch::Approx(10.0));

    CHECK(scene.render.seed == 0U);
    CHECK(static_cast<double>(scene.render.tone_map.exposure) == Catch::Approx(1.0));
    CHECK(scene.render.tone_map.op == pt::ToneMapOperator::none);

    CHECK(scene.importance_targets().empty());
}

TEST_CASE("stated values reach the scene unchanged", "[scene][loader]") {
    const TempSceneDir fixture(explicit_scene);
    const pt::Scene scene = pt::load_scene(fixture.scene_path());

    check_vec3(scene.camera.lookfrom, 1, 2, 3);
    check_vec3(scene.camera.lookat, 4, 5, 6);
    check_vec3(scene.camera.vup, 0, 0, 1);
    CHECK(static_cast<double>(scene.camera.vfov) == Catch::Approx(35.0));
    CHECK(static_cast<double>(scene.camera.defocus_angle) == Catch::Approx(2.0));
    CHECK(static_cast<double>(scene.camera.focus_dist) == Catch::Approx(7.0));

    CHECK(scene.render.image_width == 320);
    CHECK(scene.render.image_height == 180);
    CHECK(scene.render.samples_per_pixel == 8);
    CHECK(scene.render.max_depth == 12);
    CHECK(scene.render.seed == 12345U);
    check_color(scene.render.background, 0.25, 0.5, 0.75);

    CHECK(static_cast<double>(scene.render.tone_map.exposure) == Catch::Approx(2.5));
    CHECK(scene.render.tone_map.op == pt::ToneMapOperator::aces);

    CHECK_FALSE(scene.importance_targets().empty());
}

TEST_CASE("geometry reaches the world, not just the parser", "[scene][loader]") {
    const TempSceneDir fixture(defaults_scene);
    const pt::Scene scene = pt::load_scene(fixture.scene_path());

    // A radius-2 sphere at the origin, seen through the BVH the loader builds.
    const pt::Aabb bounds = scene.world().bounding_box();
    check_interval(bounds.x, -2.0, 2.0);
    check_interval(bounds.y, -2.0, 2.0);
    check_interval(bounds.z, -2.0, 2.0);
}

TEST_CASE("center_end widens the sphere's bounds along its path", "[scene][loader]") {
    const TempSceneDir fixture(motion_scene);
    const pt::Scene scene = pt::load_scene(fixture.scene_path());

    const pt::Aabb bounds = scene.world().bounding_box();
    check_interval(bounds.x, -1.0, 5.0);
    check_interval(bounds.y, -1.0, 1.0);
}

TEST_CASE("a named object can be referenced from a later slot", "[scene][loader]") {
    const TempSceneDir fixture(reference_scene);
    const pt::Scene scene = pt::load_scene(fixture.scene_path());

    // The medium shares the shell's bounds, so the union is still the shell's.
    const pt::Aabb bounds = scene.world().bounding_box();
    check_interval(bounds.x, -1.0, 1.0);
    check_interval(bounds.y, -1.0, 1.0);
    check_interval(bounds.z, -1.0, 1.0);
}

TEST_CASE("composite nodes nest and accept children by name", "[scene][loader]") {
    const TempSceneDir fixture(composite_scene);
    const pt::Scene scene = pt::load_scene(fixture.scene_path());

    // The box spans [0, 2] and the named marker sphere reaches x = 11.
    const pt::Aabb bounds = scene.world().bounding_box();
    check_interval(bounds.x, 0.0, 11.0, 1e-3);
    check_interval(bounds.y, -1.0, 2.0, 1e-3);
}

TEST_CASE("an unopenable image texture warns instead of failing the load", "[scene][loader]") {
    const TempSceneDir fixture(missing_image_scene);
    const LogSilencer silence;

    CHECK_NOTHROW(pt::load_scene(fixture.scene_path()));
}

TEST_CASE("a document that is not a scene is rejected before parsing begins", "[scene][loader]") {
    check_load_error(scenes_dir / "no_such_scene.json", "", "Cannot open scene file");

    check_scene_error(R"({"version": 1,)", "", "Cannot parse scene file");
    check_scene_error(R"([])", "", "Root JSON value must be an object");

    check_scene_error(compose({.version = ""}), "", "Missing required field 'version'");
    check_scene_error(compose({.version = "2"}), "", "Unsupported scene version");
    check_scene_error(compose({.version = R"("1")"}), "", "Unsupported scene version");
}

TEST_CASE("every required top-level section is required", "[scene][loader]") {
    check_scene_error(compose({.camera = ""}), "", "Missing required field 'camera'");
    check_scene_error(compose({.render = ""}), "", "Missing required field 'render'");
    check_scene_error(compose({.materials = ""}), "", "Missing required field 'materials'");
    check_scene_error(compose({.objects = ""}), "", "Missing required field 'objects'");

    // The "must be an object/array" checks run before the location scope opens.
    check_scene_error(compose({.camera = R"("nope")"}), "", "Field 'camera' must be a JSON object");
    check_scene_error(compose({.render = R"([])"}), "", "Field 'render' must be a JSON object");
    check_scene_error(compose({.textures = R"([])"}), "", "Field 'textures' must be a JSON object");
    check_scene_error(compose({.materials = R"([])"}), "", "Field 'materials' must be a JSON object");
    check_scene_error(compose({.objects = R"({})"}), "", "Field 'objects' must be a non-empty array");
    check_scene_error(compose({.objects = R"([])"}), "", "Field 'objects' must be a non-empty array");
}

TEST_CASE("camera fields are checked for presence, shape and range", "[scene][loader]") {
    check_scene_error(compose({.camera = R"({"lookat": [0, 0, 0], "vfov": 40})"}),
                      "/camera", "Missing required field 'lookfrom'");
    check_scene_error(compose({.camera = R"({"lookfrom": [0, 0, 5], "vfov": 40})"}),
                      "/camera", "Missing required field 'lookat'");
    check_scene_error(compose({.camera = R"({"lookfrom": [0, 0, 5], "lookat": [0, 0, 0]})"}),
                      "/camera", "Missing required field 'vfov'");

    check_scene_error(compose({.camera = R"({"lookfrom": [0, 0], "lookat": [0, 0, 0], "vfov": 40})"}),
                      "/camera", "Expected 3-element array");
    check_scene_error(compose({.camera = R"({"lookfrom": "here", "lookat": [0, 0, 0], "vfov": 40})"}),
                      "/camera", "Expected 3-element array, got string");
    check_scene_error(compose({.camera = R"({"lookfrom": [0, 0, 5], "lookat": [0, 0, 0], "vup": [0, 1], "vfov": 40})"}),
                      "/camera", "Expected 3-element array");

    check_scene_error(compose({.camera = R"({"lookfrom": [0, 0, 5], "lookat": [0, 0, 0], "vfov": 0})"}),
                      "/camera", "Field 'vfov' must be within (0, 180)");
    check_scene_error(compose({.camera = R"({"lookfrom": [0, 0, 5], "lookat": [0, 0, 0], "vfov": 180})"}),
                      "/camera", "Field 'vfov' must be within (0, 180)");
    check_scene_error(compose({.camera = R"({"lookfrom": [0, 0, 5], "lookat": [0, 0, 0], "vfov": 40, "defocus_angle": -1})"}),
                      "/camera", "Field 'defocus_angle' must be non-negative");
    check_scene_error(compose({.camera = R"({"lookfrom": [0, 0, 5], "lookat": [0, 0, 0], "vfov": 40, "focus_dist": 0})"}),
                      "/camera", "Field 'focus_dist' must be positive");
}

TEST_CASE("render settings reject bad types, bad ranges and unknown operators", "[scene][loader]") {
    check_scene_error(compose({.render = R"({"height": 2, "samples_per_pixel": 1, "max_depth": 1, "background": [0, 0, 0]})"}),
                      "/render", "Missing required field 'width'");
    check_scene_error(compose({.render = R"({"width": 2, "height": 2, "samples_per_pixel": 1, "max_depth": 1})"}),
                      "/render", "Missing required field 'background'");

    check_scene_error(compose({.render = R"({"width": 1.5, "height": 2, "samples_per_pixel": 1, "max_depth": 1, "background": [0, 0, 0]})"}),
                      "/render", "Expected integer, got number");
    check_scene_error(compose({.render = R"({"width": 0, "height": 2, "samples_per_pixel": 1, "max_depth": 1, "background": [0, 0, 0]})"}),
                      "/render", "Field 'width' must be positive");
    check_scene_error(compose({.render = R"({"width": 2, "height": -2, "samples_per_pixel": 1, "max_depth": 1, "background": [0, 0, 0]})"}),
                      "/render", "Field 'height' must be positive");
    check_scene_error(compose({.render = R"({"width": 2, "height": 2, "samples_per_pixel": 0, "max_depth": 1, "background": [0, 0, 0]})"}),
                      "/render", "Field 'samples_per_pixel' must be positive");
    check_scene_error(compose({.render = R"({"width": 2, "height": 2, "samples_per_pixel": 1, "max_depth": 0, "background": [0, 0, 0]})"}),
                      "/render", "Field 'max_depth' must be positive");

    check_scene_error(compose({.render = R"({"width": 2, "height": 2, "samples_per_pixel": 1, "max_depth": 1, "background": [0, 0]})"}),
                      "/render", "Expected 3-element array");
    check_scene_error(compose({.render = R"({"width": 2, "height": 2, "samples_per_pixel": 1, "max_depth": 1, "background": [0, -0.5, 0]})"}),
                      "/render", "Colour components must be non-negative");
    check_scene_error(compose({.render = R"({"width": 2, "height": 2, "samples_per_pixel": 1, "max_depth": 1, "background": [0, 0, 0], "seed": -1})"}),
                      "/render", "Expected non-negative integer");

    check_scene_error(compose({.render = R"({"width": 2, "height": 2, "samples_per_pixel": 1, "max_depth": 1, "background": [0, 0, 0], "tone_map": 3})"}),
                      "/render/tone_map", "Field 'tone_map' must be a JSON object");
    check_scene_error(compose({.render = R"({"width": 2, "height": 2, "samples_per_pixel": 1, "max_depth": 1, "background": [0, 0, 0], "tone_map": {"exposure": 0}})"}),
                      "/render/tone_map", "Field 'exposure' must be positive");
    check_scene_error(compose({.render = R"({"width": 2, "height": 2, "samples_per_pixel": 1, "max_depth": 1, "background": [0, 0, 0], "tone_map": {"operator": "filmic"}})"}),
                      "/render/tone_map", "Unknown tone map operator 'filmic'");
}

TEST_CASE("texture definitions are checked per name", "[scene][loader]") {
    check_scene_error(compose({.textures = R"({"grey": {"type": "gradient"}})"}),
                      "/textures/grey", "Unknown texture type 'gradient'");
    check_scene_error(compose({.textures = R"({"grey": {"albedo": [1, 1, 1]}})"}),
                      "/textures/grey", "Missing required field 'type'");
    check_scene_error(compose({.textures = R"({"grey": {"type": "solid_color"}})"}),
                      "/textures/grey", "Missing required field 'albedo'");
    check_scene_error(compose({.textures = R"({"grey": {"type": "noise", "scale": 0}})"}),
                      "/textures/grey", "Field 'scale' must be positive");
    check_scene_error(compose({.textures = R"({"grey": {"type": "image", "filename": ""}})"}),
                      "/textures/grey", "Field 'filename' must not be empty");

    // A checker's children are inline textures, so their errors nest one level deeper.
    check_scene_error(compose({.textures = R"({"checks": {"type": "checker", "scale": 1, "odd": {"type": "solid_color", "albedo": [0, 0, 0]}}})"}),
                      "/textures/checks", "Missing required field 'even'");
    check_scene_error(compose({.textures = R"({"checks": {"type": "checker", "scale": 1, "even": {"type": "swirl"}, "odd": {"type": "solid_color", "albedo": [0, 0, 0]}}})"}),
                      "/textures/checks/even", "Unknown texture type 'swirl'");
    check_scene_error(compose({.textures = R"({"checks": {"type": "checker", "scale": 1, "even": {"type": "solid_color", "albedo": [0, 0, 0]}, "odd": "grey"}})"}),
                      "/textures/checks/odd", "Expected JSON object, got string");
}

TEST_CASE("material definitions are checked per name", "[scene][loader]") {
    check_scene_error(compose({.materials = R"({"grey": {"type": "plastic"}})"}),
                      "/materials/grey", "Unknown material type 'plastic'");
    check_scene_error(compose({.materials = R"({"grey": {"type": "lambertian"}})"}),
                      "/materials/grey", "Missing required field 'texture'");
    check_scene_error(compose({.materials = R"({"grey": {"type": "lambertian", "texture": "missing"}})"}),
                      "/materials/grey", "Undefined texture 'missing'");
    check_scene_error(compose({.materials = R"({"grey": {"type": "metal", "albedo": [0.8, 0.8, 0.8], "fuzz": 1.5}})"}),
                      "/materials/grey", "Field 'fuzz' must be within [0, 1]");
    check_scene_error(compose({.materials = R"({"grey": {"type": "dielectric", "refraction_index": 0}})"}),
                      "/materials/grey", "Field 'refraction_index' must be positive");
}

TEST_CASE("object definitions are checked per index", "[scene][loader]") {
    check_scene_error(compose({.objects = R"([{"type": "torus"}])"}),
                      "/objects/0", "Unknown object type 'torus'");
    check_scene_error(compose({.objects = R"(["grey"])"}),
                      "/objects/0", "Top-level entries in 'objects' must be definitions");

    check_scene_error(compose({.objects = R"([{"type": "sphere", "center": [0, 0, 0], "radius": 0, "material": "grey"}])"}),
                      "/objects/0", "Field 'radius' must be positive");
    check_scene_error(compose({.objects = R"([{"type": "sphere", "center": [0, 0, 0], "radius": 1, "material": "gold"}])"}),
                      "/objects/0", "Undefined material 'gold'");
    check_scene_error(compose({.objects = R"([{"type": "quad", "q": [0, 0, 0], "u": [1, 0, 0], "material": "grey"}])"}),
                      "/objects/0", "Missing required field 'v'");
    check_scene_error(compose({.objects = R"([{"type": "box", "a": [0, 0, 0], "material": "grey"}])"}),
                      "/objects/0", "Missing required field 'b'");
    check_scene_error(compose({.objects = R"([{"type": "group", "children": []}])"}),
                      "/objects/0", "Field 'children' must be a non-empty array");
    check_scene_error(compose({.objects = R"([{"type": "constant_medium", "boundary": {"type": "sphere", "center": [0, 0, 0], "radius": 1, "material": "grey"}, "density": 0, "phase_function": "grey"}])"}),
                      "/objects/0", "Field 'density' must be positive");

    // The index is the second object's, not the first's.
    check_scene_error(compose({.objects = R"([{"type": "sphere", "center": [0, 0, 0], "radius": 1, "material": "grey"}, {"type": "sphere", "center": [3, 0, 0], "radius": -1, "material": "grey"}])"}),
                      "/objects/1", "Field 'radius' must be positive");
}

TEST_CASE("the location path is woven from every level it unwinds through", "[scene][loader]") {
    check_scene_error(compose({.objects = R"([{"type": "translate", "offset": [0, 0, 0], "object": {"type": "box", "a": [0, 0, 0], "b": [1, 1, 1]}}])"}),
                      "/objects/0/object", "Missing required field 'material'");
    check_scene_error(compose({.objects = R"([{"type": "translate", "offset": [0, 0, 0], "object": {"type": "rotate_y", "angle": 15, "object": {"type": "sphere", "center": [0, 0, 0], "radius": -1, "material": "grey"}}}])"}),
                      "/objects/0/object/object", "Field 'radius' must be positive");
    check_scene_error(compose({.objects = R"([{"type": "group", "children": [{"type": "sphere", "center": [0, 0, 0], "radius": 1, "material": "grey"}, {"type": "sphere", "center": [3, 0, 0], "radius": 1, "material": "chrome"}]}])"}),
                      "/objects/0/children/1", "Undefined material 'chrome'");
    check_scene_error(compose({.objects = R"([{"type": "constant_medium", "boundary": {"type": "sphere", "center": [0, 0, 0]}, "density": 1, "phase_function": "grey"}])"}),
                      "/objects/0/boundary", "Missing required field 'radius'");

    // Names resolve against what the single pass has already built, so a
    // reference that precedes its definition cannot be satisfied.
    check_scene_error(compose({.objects = R"([{"type": "translate", "offset": [0, 0, 0], "object": "later"}, {"type": "sphere", "name": "later", "center": [0, 0, 0], "radius": 1, "material": "grey"}])"}),
                      "/objects/0/object", "Undefined object 'later'");
}

TEST_CASE("importance targets must name objects that can be sampled", "[scene][loader]") {
    check_scene_error(compose({.importance_targets = R"("grey")"}),
                      "", "Field 'importance_targets' must be an array");
    check_scene_error(compose({.importance_targets = R"([7])"}),
                      "/importance_targets/0", "Expected string, got number");
    check_scene_error(compose({.importance_targets = R"(["ghost"])"}),
                      "/importance_targets/0", "Undefined object 'ghost'");

    // A box is a HittableList, and a group or a wrapper is not Sampleable either.
    check_scene_error(compose({.objects = R"([{"type": "box", "name": "crate", "a": [0, 0, 0], "b": [1, 1, 1], "material": "grey"}])",
                               .importance_targets = R"(["crate"])"}),
                      "/importance_targets/0", "is not sampleable");
    check_scene_error(compose({.objects = R"([{"type": "translate", "name": "moved", "offset": [1, 0, 0], "object": {"type": "sphere", "center": [0, 0, 0], "radius": 1, "material": "grey"}}])",
                               .importance_targets = R"(["moved"])"}),
                      "/importance_targets/0", "is not sampleable");
}
