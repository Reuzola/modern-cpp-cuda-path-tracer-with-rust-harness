#include "pt/math/color.hpp"
#include "pt/math/scalar.hpp"
#include "pt/post/tonemap.hpp"
#include "pt/render/film.hpp"
#include "support/test_support.hpp"
#include <array>
#include <catch2/catch_test_macros.hpp>
#include <limits>

namespace {

using pt::Color;
using pt::Film;
using pt::Float;
using pt::tone_map;
using pt::ToneMapOperator;
using pt::ToneMapSettings;
using pt::operator""_f;
using pt_test::require_color_near;
using pt_test::require_near;

/// A one-pixel film holding a single value on all three channels.
[[nodiscard]] Film grey(Float value) {
    Film film(1, 1);
    film.set_pixel(0, 0, Color(value, value, value));
    return film;
}

/// The red channel of the only pixel, after mapping.
[[nodiscard]] Float mapped(Float value, const ToneMapSettings& settings) {
    return tone_map(grey(value), settings).pixel(0, 0).r();
}

constexpr ToneMapSettings plain{.exposure = 1.0_f, .op = ToneMapOperator::none};

} // namespace

TEST_CASE("mapping produces a new film of the same shape", "[post][tonemap]") {
    Film source(3, 2);
    source.set_pixel(0, 0, Color(0.5_f, 0.25_f, 0.125_f));
    source.set_pixel(2, 1, Color(1.0_f, 1.0_f, 1.0_f));

    const Film result = tone_map(source, plain);

    REQUIRE(result.width() == 3);
    REQUIRE(result.height() == 2);

    // A pure function over the film: the accumulator's linear values stay
    // available, which is what lets the same render be written to both an EXR and
    // a PNG without rendering twice.
    require_color_near(source.pixel(0, 0), Color(0.5_f, 0.25_f, 0.125_f));

    // Every pixel is visited, not just the first row.
    REQUIRE(result.pixel(2, 1).r() > 0.9_f);
    require_near(result.pixel(1, 0).r(), 0.0_f);
}

TEST_CASE("channels are mapped independently", "[post][tonemap]") {
    Film source(1, 1);
    source.set_pixel(0, 0, Color(0.5_f, 0.0_f, 1.0_f));

    const Color result = tone_map(source, plain).pixel(0, 0);

    // No luminance coupling anywhere in the chain: a saturated channel does not
    // drag its neighbours with it. That is a real design choice - it is what
    // makes bright colours shift hue as they clip.
    require_near(result.r(), 0.7353_f, 1e-3);
    require_near(result.g(), 0.0_f);
    require_near(result.b(), 0.999_f);
}

TEST_CASE("the 'none' operator still encodes and clamps", "[post][tonemap]") {
    // Not an identity. `none` means "no dynamic range compression"; the transfer
    // function and the display clamp are not optional, because the bytes written
    // to a PNG are read back by a display that expects sRGB.
    require_near(mapped(0.5_f, plain), 0.7353_f, 1e-3);
    require_near(mapped(1.0_f, plain), 0.999_f);
    require_near(mapped(0.0_f, plain), 0.0_f);
}

TEST_CASE("the sRGB curve has a linear segment near black", "[post][tonemap]") {
    // Below the cutoff the encoding is a straight line of slope 12.92. The power
    // function's derivative is unbounded at zero, so without this segment the
    // darkest few codes would be spread over a vanishing range of intensities and
    // band visibly in eight bits.
    require_near(mapped(0.001_f, plain), 0.01292_f, 1e-4);
    require_near(mapped(0.002_f, plain), 0.02584_f, 1e-4);

    // The two branches meet at the cutoff: the pieces are chosen so the curve is
    // continuous, and a mismatch here would show as a step in a dark gradient.
    constexpr Float cutoff = 0.0031308_f;
    const Float from_below = mapped(cutoff * 0.999_f, plain);
    const Float at_cutoff = mapped(cutoff, plain);
    require_near(from_below, at_cutoff, 1e-3);
}

TEST_CASE("negative and non-finite samples become black", "[post][tonemap]") {
    // A NaN can arrive from a degenerate PDF ratio or a zero-length direction
    // deep in a path. Left alone it would propagate through the writer and land
    // in the file as an arbitrary byte - a single bright speck that no amount of
    // extra samples removes.
    require_near(mapped(std::numeric_limits<Float>::quiet_NaN(), plain), 0.0_f);

    // Sanitising happens before the exposure multiply, so scaling a NaN never
    // gets a chance to produce another one.
    require_near(mapped(std::numeric_limits<Float>::quiet_NaN(), {.exposure = 4.0_f, .op = ToneMapOperator::aces}), 0.0_f);

    // Negative radiance is not physical but is reachable through a numerically
    // unlucky estimator. It floors to black before any operator sees it - both
    // reinhard and aces map a large negative to a positive value, so the answer
    // would otherwise depend on which operator the scene happened to pick.
    require_near(mapped(-0.5_f, plain), 0.0_f);

    const std::array operators = {ToneMapOperator::none, ToneMapOperator::reinhard, ToneMapOperator::aces};
    for (const ToneMapOperator op : operators) {
        require_near(mapped(-1000.0_f, {.exposure = 2.0_f, .op = op}), 0.0_f);
    }
}

TEST_CASE("the display clamp stops just short of one", "[post][tonemap]") {
    // The ceiling is 0.999, not 1.0, and that is deliberate: the byte conversion
    // multiplies by 256 and truncates, so 0.999 lands on 255 while 1.0 would
    // overflow to 256 and wrap to zero. The two constants live in different files
    // and only make sense together - io/color.cpp asserts the upper bound this
    // function guarantees.
    require_near(mapped(2.0_f, plain), 0.999_f);
    require_near(mapped(1e6_f, {.exposure = 1000.0_f, .op = ToneMapOperator::none}), 0.999_f);
}

TEST_CASE("exposure scales the scene, not the mapped image", "[post][tonemap]") {
    const ToneMapSettings exposed{.exposure = 2.0_f, .op = ToneMapOperator::reinhard};

    // 0.5 doubled to 1.0, compressed by reinhard to 0.5, encoded to 0.7353.
    // Applying the exposure after the operator would give reinhard(0.5) * 2 =
    // 0.6667 and an encoded 0.8385 - brighter, plausible, and wrong. Exposure is
    // a camera setting: it belongs in front of the response curve, exactly as a
    // longer shutter collects more light before the film reacts to it.
    require_near(mapped(0.5_f, exposed), 0.7353_f, 1e-3);

    // With no operator in the way, exposure is a plain multiply on the linear
    // value: 0.25 * 2 = 0.5.
    require_near(mapped(0.25_f, {.exposure = 2.0_f, .op = ToneMapOperator::none}), 0.7353_f, 1e-3);
}

TEST_CASE("reinhard compresses without ever reaching white", "[post][tonemap]") {
    const ToneMapSettings settings{.exposure = 1.0_f, .op = ToneMapOperator::reinhard};

    // x / (1 + x): fixed at zero, half at one, asymptotic to one.
    require_near(mapped(0.0_f, settings), 0.0_f);
    require_near(mapped(1.0_f, settings), 0.7353_f, 1e-3);

    // A very bright highlight still lands below the clamp rather than on it, so
    // the relative brightness of two highlights survives into the image instead
    // of flattening into one white blob.
    const Float bright = mapped(100.0_f, settings);
    const Float brighter = mapped(1000.0_f, settings);
    REQUIRE(bright < 0.999_f);
    REQUIRE(brighter > bright);
}

TEST_CASE("aces keeps black at black and holds up the midtones", "[post][tonemap]") {
    const ToneMapSettings settings{.exposure = 1.0_f, .op = ToneMapOperator::aces};

    // The fitted curve has a numerator that vanishes at zero while its
    // denominator does not, so black stays black instead of lifting to grey.
    require_near(mapped(0.0_f, settings), 0.0_f);

    // At a linear one the fit gives 2.54 / 3.16 = 0.8038, encoded to 0.9082.
    require_near(mapped(1.0_f, settings), 0.9082_f, 1e-3);

    // Above reinhard through the midrange: reinhard's single ratio compresses
    // everything at once, so it pulls midtones down along with the highlights.
    // The fitted aces curve spends its compression on the shoulder instead and
    // leaves the middle of the range close to where it was - which is the point
    // of a filmic curve and the reason it is the default for most renderers.
    REQUIRE(mapped(0.5_f, settings) > mapped(0.5_f, {.exposure = 1.0_f, .op = ToneMapOperator::reinhard}));
}

TEST_CASE("every operator is monotone", "[post][tonemap]") {
    const std::array operators = {ToneMapOperator::none, ToneMapOperator::reinhard, ToneMapOperator::aces};

    for (const ToneMapOperator op : operators) {
        const ToneMapSettings settings{.exposure = 1.0_f, .op = op};

        Float previous = -1.0_f;
        for (int i = 0; i <= 40; ++i) {
            const Float input = static_cast<Float>(i) * 0.1_f;
            const Float output = mapped(input, settings);

            // Brighter in must never be darker out. A non-monotone curve inverts
            // contrast somewhere in its range, which reads as an outline around
            // bright objects and is far easier to see than to explain.
            REQUIRE(output >= previous);
            previous = output;
        }
    }
}
