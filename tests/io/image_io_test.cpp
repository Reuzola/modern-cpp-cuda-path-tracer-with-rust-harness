#include "pt/io/color.hpp"
#include "pt/io/exr_writer.hpp"
#include "pt/io/image_format.hpp"
#include "pt/io/image_loader.hpp"
#include "pt/io/image_writer.hpp"
#include "pt/io/png_writer.hpp"
#include "pt/io/ppm_writer.hpp"
#include "pt/math/color.hpp"
#include "pt/math/scalar.hpp"
#include "pt/post/tonemap.hpp"
#include "pt/render/film.hpp"
#include "support/clog_capture.hpp"
#include "support/log_silencer.hpp"
#include "support/temp_dir.hpp"
#include "support/test_support.hpp"
#include <array>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>

namespace {

using pt::Color;
using pt::ExrWriter;
using pt::Film;
using pt::Float;
using pt::ImageFormat;
using pt::ImageLoader;
using pt::ImageWriter;
using pt::PngWriter;
using pt::PpmWriter;
using pt::ToneMapOperator;
using pt::ToneMapSettings;
using pt::make_image_writer;
using pt::to_ldr_bytes;
using pt::tone_map;
using pt::operator""_f;
using pt_test::ClogCapture;
using pt_test::LogSilencer;
using pt_test::TempDir;
using pt_test::require_color_near;

[[nodiscard]] std::string read_file(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ifstream::binary);
    REQUIRE(stream.good());
    return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

/// A 2x2 film whose channels are only ever 0 or 0.999.
///
/// Those two map to the byte extremes, which survive any transfer function the
/// decoder might apply on the way back in. stb decodes an 8-bit file with a 2.2
/// gamma while the engine encodes with the sRGB curve, so a mid-grey would not
/// round-trip exactly - a real mismatch, but not one this fixture should depend
/// on.
[[nodiscard]] Film corners() {
    Film film(2, 2);
    film.set_pixel(0, 0, Color(0.999_f, 0.0_f, 0.0_f));
    film.set_pixel(1, 0, Color(0.0_f, 0.999_f, 0.0_f));
    film.set_pixel(0, 1, Color(0.0_f, 0.0_f, 0.999_f));
    film.set_pixel(1, 1, Color(0.999_f, 0.999_f, 0.999_f));
    return film;
}

} // namespace

TEST_CASE("a display value becomes a byte by truncation", "[io][color]") {
    // 256 rather than 255, then truncated: every code gets an equal share of the
    // input range, instead of only 1.0 mapping to 255.
    REQUIRE(to_ldr_bytes(Color(0.0_f, 0.25_f, 0.5_f)) == std::array<std::uint8_t, 3>{0, 64, 128});
    REQUIRE(to_ldr_bytes(Color(0.999_f, 0.999_f, 0.999_f)) == std::array<std::uint8_t, 3>{255, 255, 255});

    // The upper limit is a precondition, not a clamp: values at or above one
    // would overflow the byte, and the debug build asserts rather than wrapping.
    // Nothing reaches this function without passing through tone mapping first.
}

TEST_CASE("tone mapping's ceiling and the byte conversion agree", "[io][color]") {
    // The 0.999 clamp in post/ and the multiply by 256 here are one contract
    // split across two files. Whatever the scene's radiance, the brightest byte
    // is 255 and never wraps to zero.
    const ToneMapSettings settings{.exposure = 1000.0_f, .op = ToneMapOperator::none};

    Film film(1, 1);
    film.set_pixel(0, 0, Color(1e6_f, 1e6_f, 1e6_f));

    const Film mapped = tone_map(film, settings);
    REQUIRE(to_ldr_bytes(mapped.pixel(0, 0)) == std::array<std::uint8_t, 3>{255, 255, 255});

    // And the floor, which the same assert guards from below.
    Film dark(1, 1);
    dark.set_pixel(0, 0, Color(-5.0_f, 0.0_f, 0.0_f));
    REQUIRE(to_ldr_bytes(tone_map(dark, settings).pixel(0, 0)) == std::array<std::uint8_t, 3>{0, 0, 0});
}

TEST_CASE("the factory returns the writer the format names", "[io][writer]") {
    const std::unique_ptr<ImageWriter> ppm = make_image_writer(ImageFormat::ppm);
    const std::unique_ptr<ImageWriter> png = make_image_writer(ImageFormat::png);
    const std::unique_ptr<ImageWriter> exr = make_image_writer(ImageFormat::exr);

    // dynamic_cast rather than a behavioural check: the mapping from format to
    // implementation is the whole content of this function, and a swapped pair
    // would otherwise only show up as a PNG with a .ppm extension.
    REQUIRE(dynamic_cast<const PpmWriter*>(ppm.get()) != nullptr);
    REQUIRE(dynamic_cast<const PngWriter*>(png.get()) != nullptr);
    REQUIRE(dynamic_cast<const ExrWriter*>(exr.get()) != nullptr);
}

TEST_CASE("only EXR carries high dynamic range", "[io][writer]") {
    // Resolved at compile time, and consulted before rendering: an HDR format
    // skips tone mapping entirely and writes the accumulator's linear values.
    STATIC_REQUIRE_FALSE(pt::is_hdr(ImageFormat::ppm));
    STATIC_REQUIRE_FALSE(pt::is_hdr(ImageFormat::png));
    STATIC_REQUIRE(pt::is_hdr(ImageFormat::exr));
}

TEST_CASE("the PPM writer emits a plain P3 file", "[io][ppm]") {
    const TempDir dir("pt_io");
    const std::filesystem::path path = dir.path() / "out.ppm";

    Film film(2, 2);
    film.set_pixel(0, 0, Color(0.0_f, 0.5_f, 0.999_f));
    film.set_pixel(1, 0, Color(0.999_f, 0.0_f, 0.5_f));
    film.set_pixel(0, 1, Color(0.25_f, 0.25_f, 0.25_f));
    film.set_pixel(1, 1, Color(0.0_f, 0.0_f, 0.0_f));

    REQUIRE(PpmWriter{}.write(film, path));

    // Byte for byte: the format is the one thing about PPM worth having, and it
    // is small enough to state exactly. Scanlines run top to bottom and pixels
    // left to right, matching the film's own indexing.
    REQUIRE(read_file(path) == "P3\n2 2\n255\n0 128 255\n255 0 128\n64 64 64\n0 0 0\n");
}

TEST_CASE("a writer reports a path it cannot open", "[io][writer]") {
    const TempDir dir("pt_io");
    const std::filesystem::path missing = dir.path() / "no_such_directory" / "out.ppm";

    const LogSilencer silence;

    // A failed write is a return value, not an exception and not a crash: the
    // renderer has already spent minutes producing the film and needs to say so
    // rather than lose it.
    REQUIRE_FALSE(PpmWriter{}.write(Film(2, 2), missing));
    REQUIRE_FALSE(PngWriter{}.write(Film(2, 2), dir.path() / "no_such_directory" / "out.png"));
    REQUIRE_FALSE(ExrWriter{}.write(Film(2, 2), dir.path() / "no_such_directory" / "out.exr"));
}

TEST_CASE("a PNG survives a round trip through the loader", "[io][png]") {
    const TempDir dir("pt_io");
    const std::filesystem::path path = dir.path() / "out.png";

    REQUIRE(PngWriter{}.write(corners(), path));

    const ImageLoader loaded(path.string());
    REQUIRE(loaded.width() == 2);
    REQUIRE(loaded.height() == 2);

    // Row zero is the top scanline on both sides of the trip. If the writer and
    // the loader disagreed here, an image texture would come back mirrored and
    // nothing in the pipeline would complain.
    require_color_near(loaded.pixel_data(0, 0), Color(1, 0, 0), 1e-2);
    require_color_near(loaded.pixel_data(1, 0), Color(0, 1, 0), 1e-2);
    require_color_near(loaded.pixel_data(0, 1), Color(0, 0, 1), 1e-2);
    require_color_near(loaded.pixel_data(1, 1), Color(1, 1, 1), 1e-2);
}

TEST_CASE("pixel access is clamped to the image", "[io][loader]") {
    const TempDir dir("pt_io");
    const std::filesystem::path path = dir.path() / "out.png";
    REQUIRE(PngWriter{}.write(corners(), path));

    const ImageLoader loaded(path.string());

    // The texture computes a column from a uv of exactly one and lands one past
    // the last column, so the clamp is on the normal path rather than a guard
    // against nonsense.
    require_color_near(loaded.pixel_data(2, 0), loaded.pixel_data(1, 0), 1e-2);
    require_color_near(loaded.pixel_data(-1, -1), loaded.pixel_data(0, 0), 1e-2);
    require_color_near(loaded.pixel_data(99, 99), loaded.pixel_data(1, 1), 1e-2);
}

TEST_CASE("a missing file loads as an empty image and says so", "[io][loader]") {
    const TempDir dir("pt_io");
    const std::string path = (dir.path() / "absent.png").string();

    std::string log_text;
    {
        const ClogCapture capture;
        const ImageLoader loaded(path);

        REQUIRE(loaded.width() == 0);
        REQUIRE(loaded.height() == 0);

        // Safe to call even with nothing loaded: the clamps would otherwise be
        // handed an inverted range and the buffer pointer is null.
        require_color_near(loaded.pixel_data(0, 0), Color(0, 0, 0));
        require_color_near(loaded.pixel_data(50, 50), Color(0, 0, 0));

        log_text = capture.text();
    }

    // A missing texture is a warning, not a load failure: the scene still renders
    // and the message names the file so the mistake is findable.
    REQUIRE_THAT(log_text, Catch::Matchers::ContainsSubstring("absent.png"));
    REQUIRE_THAT(log_text, Catch::Matchers::StartsWith("warning: "));
}

TEST_CASE("the EXR writer produces a real EXR", "[io][exr]") {
    const TempDir dir("pt_io");
    const std::filesystem::path path = dir.path() / "out.exr";

    Film film(4, 3);

    // Above one on purpose: this is the format that keeps it. The value cannot be
    // read back here - OpenEXR is a private dependency of the engine - so the
    // check is that the file is well-formed and non-trivial.
    film.set_pixel(0, 0, Color(12.5_f, 0.0_f, 3.25_f));

    REQUIRE(ExrWriter{}.write(film, path));

    const std::string bytes = read_file(path);
    REQUIRE(bytes.size() > 32);

    // The format's magic number, 0x76 0x2f 0x31 0x01.
    REQUIRE(static_cast<unsigned char>(bytes[0]) == 0x76);
    REQUIRE(static_cast<unsigned char>(bytes[1]) == 0x2f);
    REQUIRE(static_cast<unsigned char>(bytes[2]) == 0x31);
    REQUIRE(static_cast<unsigned char>(bytes[3]) == 0x01);
}
