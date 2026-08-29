#include "pt/render/progress.hpp"
#include "support/clog_capture.hpp"
#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <string>

namespace {

using pt::ConsoleProgressReporter;
using pt::RenderProgress;

} // namespace

TEST_CASE("the fraction is safe on an empty render", "[render][progress]") {
    STATIC_REQUIRE(RenderProgress{0, 10}.fraction() == 0.0);
    STATIC_REQUIRE(RenderProgress{5, 10}.fraction() == 0.5);
    STATIC_REQUIRE(RenderProgress{10, 10}.fraction() == 1.0);

    // A total of zero is a division by zero waiting to happen; it answers zero
    // instead. Computed in double even though Float may be float: this is a
    // display quantity, and rounding it early would make 999 of 1000 passes read
    // as a hundred percent.
    STATIC_REQUIRE(RenderProgress{0, 0}.fraction() == 0.0);
    STATIC_REQUIRE(RenderProgress{3, 0}.fraction() == 0.0);
}

TEST_CASE("the console reporter writes only when the percentage moves", "[render][progress]") {
    ConsoleProgressReporter reporter;
    const pt_test::ClogCapture capture;

    // Two hundred passes over a hundred percentage points: half the reports say
    // nothing new. A render can call this tens of thousands of times, and a
    // terminal is slow enough for that to become a measurable share of a short
    // render.
    for (int i = 0; i <= 200; ++i) {
        reporter(RenderProgress{i, 200});
    }

    const std::string text = capture.text();
    REQUIRE_THAT(text, Catch::Matchers::ContainsSubstring("100%"));
    REQUIRE_THAT(text, Catch::Matchers::ContainsSubstring("(200 / 200 passes)"));

    // 101 distinct percentages, each one line's worth of output.
    const auto returns = std::ranges::count(text, '\r');
    REQUIRE(returns == 101);
}

TEST_CASE("the reporter rewrites one line and ends it once", "[render][progress]") {
    ConsoleProgressReporter reporter;
    const pt_test::ClogCapture capture;

    reporter(RenderProgress{0, 4});
    reporter(RenderProgress{2, 4});
    reporter(RenderProgress{4, 4});

    const std::string text = capture.text();

    // Carriage returns rather than newlines: the bar overwrites itself in place,
    // so a finished render leaves one line behind instead of a screenful.
    REQUIRE(std::ranges::count(text, '\r') == 3);

    // Exactly one newline, at the end, so whatever the program prints next starts
    // on a clean line.
    REQUIRE(std::ranges::count(text, '\n') == 1);
    REQUIRE(text.back() == '\n');
}

TEST_CASE("progress on stderr keeps the image stream clean", "[render][progress]") {
    ConsoleProgressReporter reporter;

    const pt_test::ClogCapture capture;
    reporter(RenderProgress{1, 2});

    // Written to clog, not cout. The renderer can stream a PPM to standard output
    // and be piped somewhere while the progress bar still reaches the terminal.
    REQUIRE_FALSE(capture.text().empty());
}
