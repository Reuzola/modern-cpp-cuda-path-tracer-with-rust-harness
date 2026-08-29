#include "pt/io/png_writer.hpp"
#include "pt/math/color.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include "pt/render/film.hpp"
#include "pt/textures/image_texture.hpp"
#include "support/log_silencer.hpp"
#include "support/temp_dir.hpp"
#include "support/test_support.hpp"
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <string>

namespace {

using pt::Color;
using pt::Film;
using pt::ImageTexture;
using pt::PngWriter;
using pt::Point3;
using pt::operator""_f;
using pt_test::LogSilencer;
using pt_test::TempDir;
using pt_test::require_color_near;

// PT_ASSETS_DIR is injected by tests/CMakeLists.txt.
const std::filesystem::path assets_dir{PT_ASSETS_DIR};

const Point3 anywhere{7, -3, 11};

} // namespace

TEST_CASE("a missing image renders as a visible sentinel", "[textures][image]") {
    const LogSilencer silence;
    const ImageTexture texture{"definitely_not_a_file.png"};

    // Cyan, not black or magenta-checkered: bright enough to be noticed
    // immediately and impossible to mistake for a material someone chose. The
    // scene still loads, so a missing texture costs a render rather than a run.
    require_color_near(texture.value(0.5_f, 0.5_f, anywhere), Color(0, 1, 1));
    require_color_near(texture.value(0.0_f, 0.0_f, anywhere), Color(0, 1, 1));
}

TEST_CASE("v runs upwards while image rows run downwards", "[textures][image]") {
    const TempDir dir("pt_image");
    const std::filesystem::path path = dir.path() / "quadrants.png";

    // A 2x2 image: red and green across the top row, blue and white across the
    // bottom. Only the byte extremes are used, so the decoder's transfer function
    // does not enter into it.
    Film film(2, 2);
    film.set_pixel(0, 0, Color(0.999_f, 0.0_f, 0.0_f));
    film.set_pixel(1, 0, Color(0.0_f, 0.999_f, 0.0_f));
    film.set_pixel(0, 1, Color(0.0_f, 0.0_f, 0.999_f));
    film.set_pixel(1, 1, Color(0.999_f, 0.999_f, 0.999_f));
    REQUIRE(PngWriter{}.write(film, path));

    const ImageTexture texture{path.string()};

    // Texture space puts v = 0 at the bottom, image files put row 0 at the top,
    // so the lookup flips. This is the convention that decides whether a world
    // map arrives upside down, and nothing else in the engine states it.
    require_color_near(texture.value(0.25_f, 0.75_f, anywhere), Color(1, 0, 0), 1e-2);
    require_color_near(texture.value(0.75_f, 0.75_f, anywhere), Color(0, 1, 0), 1e-2);
    require_color_near(texture.value(0.25_f, 0.25_f, anywhere), Color(0, 0, 1), 1e-2);
    require_color_near(texture.value(0.75_f, 0.25_f, anywhere), Color(1, 1, 1), 1e-2);
}

TEST_CASE("uv outside the unit square clamps to the edge", "[textures][image]") {
    const TempDir dir("pt_image");
    const std::filesystem::path path = dir.path() / "quadrants.png";

    Film film(2, 2);
    film.set_pixel(0, 0, Color(0.999_f, 0.0_f, 0.0_f));
    film.set_pixel(1, 0, Color(0.0_f, 0.999_f, 0.0_f));
    film.set_pixel(0, 1, Color(0.0_f, 0.0_f, 0.999_f));
    film.set_pixel(1, 1, Color(0.999_f, 0.999_f, 0.999_f));
    REQUIRE(PngWriter{}.write(film, path));

    const ImageTexture texture{path.string()};

    // Clamped rather than wrapped: a sphere's u seam would tile the image if it
    // wrapped, and a mesh with uv slightly outside the range would show a jump
    // instead of a stretched edge pixel.
    require_color_near(texture.value(-3.0_f, 0.75_f, anywhere), texture.value(0.25_f, 0.75_f, anywhere), 1e-2);
    require_color_near(texture.value(5.0_f, 0.75_f, anywhere), texture.value(0.75_f, 0.75_f, anywhere), 1e-2);

    // Exactly one on either coordinate lands one row or column past the end and
    // is caught by the loader's own clamp - the two clamps are both load-bearing.
    require_color_near(texture.value(1.0_f, 1.0_f, anywhere), Color(0, 1, 0), 1e-2);
    require_color_near(texture.value(0.0_f, 0.0_f, anywhere), Color(0, 0, 1), 1e-2);
}

TEST_CASE("an image texture ignores the world position", "[textures][image]") {
    const TempDir dir("pt_image");
    const std::filesystem::path path = dir.path() / "flat.png";

    Film film(2, 2);
    film.set_pixel(0, 0, Color(0.999_f, 0.0_f, 0.0_f));
    film.set_pixel(1, 0, Color(0.999_f, 0.0_f, 0.0_f));
    film.set_pixel(0, 1, Color(0.999_f, 0.0_f, 0.0_f));
    film.set_pixel(1, 1, Color(0.999_f, 0.0_f, 0.0_f));
    REQUIRE(PngWriter{}.write(film, path));

    const ImageTexture texture{path.string()};

    // Surface-parameterised, unlike the checker and noise textures: the pattern
    // travels with the object rather than staying fixed in the world.
    require_color_near(texture.value(0.5_f, 0.5_f, Point3(0, 0, 0)), texture.value(0.5_f, 0.5_f, Point3(-40, 12, 6)), 1e-2);
}

TEST_CASE("a real asset decodes into a usable range", "[textures][image]") {
    const ImageTexture earth{(assets_dir / "earthmap.jpg").string()};

    // The JPEG path through stb, which no other test exercises: the round-trip
    // fixtures above are all PNG. A decoder that failed would come back cyan.
    const Color sample = earth.value(0.5_f, 0.5_f, anywhere);
    REQUIRE_FALSE((sample.r() == 0.0_f && sample.g() == 1.0_f && sample.b() == 1.0_f));

    for (int i = 0; i <= 10; ++i) {
        const pt::Float u = static_cast<pt::Float>(i) / 10.0_f;
        const Color value = earth.value(u, 0.5_f, anywhere);

        // Decoded to linear floats in [0, 1]: an albedo above one would add
        // energy at every bounce.
        REQUIRE(value.r() >= 0.0_f);
        REQUIRE(value.r() <= 1.0_f);
        REQUIRE(value.g() >= 0.0_f);
        REQUIRE(value.g() <= 1.0_f);
        REQUIRE(value.b() >= 0.0_f);
        REQUIRE(value.b() <= 1.0_f);
    }
}
