#include "pt/math/color.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include "pt/textures/checker_texture.hpp"
#include "pt/textures/solid_color.hpp"
#include "support/probe_texture.hpp"
#include "support/test_support.hpp"
#include <catch2/catch_test_macros.hpp>

namespace {

using pt::CheckerTexture;
using pt::Color;
using pt::Point3;
using pt::SolidColor;
using pt::operator""_f;
using pt_test::ProbeTexture;
using pt_test::require_color_near;
using pt_test::require_near;

const Color white{1.0_f, 1.0_f, 1.0_f};
const Color black{0.0_f, 0.0_f, 0.0_f};

} // namespace

TEST_CASE("a solid colour ignores where it is asked about", "[textures][solid_color]") {
    const SolidColor red{Color(1.0_f, 0.0_f, 0.0_f)};

    require_color_near(red.value(0.0_f, 0.0_f, Point3(0, 0, 0)), Color(1.0_f, 0.0_f, 0.0_f));
    require_color_near(red.value(0.7_f, 0.3_f, Point3(9, -4, 2)), Color(1.0_f, 0.0_f, 0.0_f));

    // The channel constructor is a convenience over the Color one, not a second
    // implementation.
    const SolidColor channels{0.25_f, 0.5_f, 0.75_f};
    require_color_near(channels.value(0.0_f, 0.0_f, Point3(0, 0, 0)), Color(0.25_f, 0.5_f, 0.75_f));
}

TEST_CASE("the checker alternates on every axis", "[textures][checker]") {
    const SolidColor even_tex{white};
    const SolidColor odd_tex{black};
    const CheckerTexture checker{1.0_f, &even_tex, &odd_tex};

    // Cells are unit cubes at this scale, and the parity is the sum of the three
    // cell indices - so stepping one cell along any single axis flips the colour.
    require_color_near(checker.value(0, 0, Point3(0.5_f, 0.5_f, 0.5_f)), white);
    require_color_near(checker.value(0, 0, Point3(1.5_f, 0.5_f, 0.5_f)), black);
    require_color_near(checker.value(0, 0, Point3(0.5_f, 1.5_f, 0.5_f)), black);
    require_color_near(checker.value(0, 0, Point3(0.5_f, 0.5_f, 1.5_f)), black);

    // Two steps returns to the original colour.
    require_color_near(checker.value(0, 0, Point3(1.5_f, 1.5_f, 0.5_f)), white);
}

TEST_CASE("cells are half-open, so the boundary belongs to the next cell", "[textures][checker]") {
    const SolidColor even_tex{white};
    const SolidColor odd_tex{black};
    const CheckerTexture checker{1.0_f, &even_tex, &odd_tex};

    // floor() puts an exact integer coordinate at the start of the higher cell.
    require_color_near(checker.value(0, 0, Point3(0.999_f, 0.5_f, 0.5_f)), white);
    require_color_near(checker.value(0, 0, Point3(1.0_f, 0.5_f, 0.5_f)), black);
}

TEST_CASE("the pattern keeps alternating through the origin", "[textures][checker]") {
    const SolidColor even_tex{white};
    const SolidColor odd_tex{black};
    const CheckerTexture checker{1.0_f, &even_tex, &odd_tex};

    // The parity test is `sum % 2 == 0`, and in C++ a negative dividend gives a
    // negative remainder - so an odd negative sum compares unequal to zero and
    // lands on the odd branch, which is the right answer. Written down because
    // the line looks like a bug and is not: rewriting it as `% 2 != 0` or as a
    // bitmask would change the pattern on one side of the origin only.
    require_color_near(checker.value(0, 0, Point3(-0.5_f, 0.5_f, 0.5_f)), black);
    require_color_near(checker.value(0, 0, Point3(-1.5_f, 0.5_f, 0.5_f)), white);
    require_color_near(checker.value(0, 0, Point3(-2.5_f, 0.5_f, 0.5_f)), black);

    // Two negative axes at once: the sum is even and the cell is an even one.
    require_color_near(checker.value(0, 0, Point3(-0.5_f, -0.5_f, 0.5_f)), white);
}

TEST_CASE("the scale is the size of a cell", "[textures][checker]") {
    const SolidColor even_tex{white};
    const SolidColor odd_tex{black};
    const CheckerTexture checker{2.0_f, &even_tex, &odd_tex};

    // Stored as its reciprocal so the hot path multiplies instead of dividing.
    // A cell is two units wide here, so the flip that happened at x = 1 above now
    // happens at x = 2.
    require_color_near(checker.value(0, 0, Point3(1.5_f, 0.5_f, 0.5_f)), white);
    require_color_near(checker.value(0, 0, Point3(2.5_f, 0.5_f, 0.5_f)), black);
}

TEST_CASE("the checker forwards its coordinates untouched", "[textures][checker]") {
    const ProbeTexture even_tex{white};
    const ProbeTexture odd_tex{black};
    const CheckerTexture checker{1.0_f, &even_tex, &odd_tex};

    const Point3 point(0.5_f, 0.5_f, 0.5_f);
    require_color_near(checker.value(0.25_f, 0.75_f, point), white);

    // The checker decides which child answers and changes nothing else: a nested
    // image or noise texture still sees the surface's own uv. Only one child is
    // consulted per lookup, so the other's cost is never paid.
    REQUIRE(even_tex.calls() == 1);
    REQUIRE(odd_tex.calls() == 0);
    require_near(even_tex.last_u(), 0.25_f);
    require_near(even_tex.last_v(), 0.75_f);
    require_near(even_tex.last_p().x(), 0.5_f);
}

TEST_CASE("the checker is a solid texture, not a surface pattern", "[textures][checker]") {
    const SolidColor even_tex{white};
    const SolidColor odd_tex{black};
    const CheckerTexture checker{1.0_f, &even_tex, &odd_tex};

    // Driven by the world position alone. That is why the pattern is continuous
    // across two touching objects, and why it slides over a moving sphere rather
    // than travelling with it.
    require_color_near(checker.value(0.0_f, 0.0_f, Point3(0.5_f, 0.5_f, 0.5_f)), white);
    require_color_near(checker.value(0.9_f, 0.1_f, Point3(0.5_f, 0.5_f, 0.5_f)), white);
}
