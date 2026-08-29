#include "pt/math/constants.hpp"
#include "pt/math/interval.hpp"
#include "pt/math/scalar.hpp"
#include "support/test_support.hpp"
#include <catch2/catch_test_macros.hpp>

namespace {

using pt::Interval;
using pt::infinity;
using pt::operator""_f;
using pt_test::require_near;

} // namespace

TEST_CASE("a default-constructed interval is empty", "[math][interval]") {
    // Reversed bounds, not zero-width: this is the identity element for the
    // union constructor below, which is what lets a bounding box start out
    // empty and grow by absorbing primitives one at a time.
    const Interval empty;

    REQUIRE_FALSE(empty.contains(0.0_f));
    REQUIRE_FALSE(empty.contains(infinity));
    REQUIRE_FALSE(empty.contains(-infinity));
    REQUIRE_FALSE(empty.surrounds(0.0_f));

    // A negative size is the observable mark of emptiness. Aabb::surface_area
    // relies on it, clamping each extent at zero rather than producing a
    // negative area for an empty box.
    REQUIRE(empty.size() < 0.0_f);
}

TEST_CASE("contains is closed and surrounds is open", "[math][interval]") {
    const Interval unit(0.0_f, 1.0_f);

    SECTION("both agree strictly inside") {
        REQUIRE(unit.contains(0.5_f));
        REQUIRE(unit.surrounds(0.5_f));
    }
    SECTION("they disagree exactly at the endpoints") {
        REQUIRE(unit.contains(0.0_f));
        REQUIRE(unit.contains(1.0_f));
        REQUIRE_FALSE(unit.surrounds(0.0_f));
        REQUIRE_FALSE(unit.surrounds(1.0_f));
    }
    SECTION("both reject the outside") {
        REQUIRE_FALSE(unit.contains(-0.001_f));
        REQUIRE_FALSE(unit.contains(1.001_f));
        REQUIRE_FALSE(unit.surrounds(1.001_f));
    }
    SECTION("a single-point interval contains but never surrounds") {
        const Interval degenerate(2.0_f, 2.0_f);

        REQUIRE(degenerate.contains(2.0_f));
        REQUIRE_FALSE(degenerate.surrounds(2.0_f));
        REQUIRE(degenerate.size() == 0.0_f);
    }
}

TEST_CASE("clamp pulls a value onto the nearest bound", "[math][interval]") {
    const Interval range(-1.0_f, 4.0_f);

    require_near(range.clamp(2.0_f), 2.0_f);
    require_near(range.clamp(-9.0_f), -1.0_f);
    require_near(range.clamp(9.0_f), 4.0_f);

    SECTION("the endpoints are fixed points") {
        require_near(range.clamp(-1.0_f), -1.0_f);
        require_near(range.clamp(4.0_f), 4.0_f);
    }
    SECTION("clamping against an empty interval is meaningless") {
        // Documented, not endorsed. With min above max the first branch always
        // wins and every input maps to +infinity, so callers must never clamp
        // against a default-constructed interval.
        REQUIRE(Interval().clamp(0.0_f) == infinity);
    }
}

TEST_CASE("expand grows the interval symmetrically", "[math][interval]") {
    const Interval range(0.0_f, 4.0_f);
    const Interval wider = range.expand(2.0_f);

    // Half the delta goes to each side, so the midpoint does not move.
    require_near(wider.min, -1.0_f);
    require_near(wider.max, 5.0_f);
    require_near(wider.size(), range.size() + 2.0_f);

    SECTION("a zero delta is the identity") {
        require_near(range.expand(0.0_f).min, range.min);
        require_near(range.expand(0.0_f).max, range.max);
    }
    SECTION("a negative delta shrinks it") {
        const Interval narrower = range.expand(-2.0_f);

        require_near(narrower.min, 1.0_f);
        require_near(narrower.max, 3.0_f);
    }
}

TEST_CASE("the two-interval constructor takes the hull, not the set union", "[math][interval]") {
    SECTION("overlapping intervals merge into their span") {
        const Interval merged(Interval(0.0_f, 2.0_f), Interval(1.0_f, 5.0_f));

        require_near(merged.min, 0.0_f);
        require_near(merged.max, 5.0_f);
    }
    SECTION("disjoint intervals swallow the gap between them") {
        // This is the whole point of a bounding volume: the result is
        // conservative, covering space neither operand occupied.
        const Interval merged(Interval(0.0_f, 1.0_f), Interval(9.0_f, 10.0_f));

        require_near(merged.min, 0.0_f);
        require_near(merged.max, 10.0_f);
        REQUIRE(merged.contains(5.0_f));
    }
    SECTION("the empty interval is the identity element") {
        const Interval range(3.0_f, 7.0_f);

        const Interval from_left(Interval(), range);
        const Interval from_right(range, Interval());

        require_near(from_left.min, range.min);
        require_near(from_left.max, range.max);
        require_near(from_right.min, range.min);
        require_near(from_right.max, range.max);
    }
    SECTION("merging two empty intervals stays empty") {
        REQUIRE(Interval(Interval(), Interval()).size() < 0.0_f);
    }
}

TEST_CASE("universe is unbounded in both directions", "[math][interval]") {
    // Declared const inside the class and defined constexpr outside it - the
    // only way a class can hold a static member of its own type. This case
    // confirms the definition is actually linked, not just declared.
    REQUIRE(Interval::universe.contains(0.0_f));
    REQUIRE(Interval::universe.contains(1.0e30_f));
    REQUIRE(Interval::universe.contains(-1.0e30_f));

    SECTION("the infinities are inside but not surrounded") {
        REQUIRE(Interval::universe.contains(infinity));
        REQUIRE(Interval::universe.contains(-infinity));
        REQUIRE_FALSE(Interval::universe.surrounds(infinity));
        REQUIRE_FALSE(Interval::universe.surrounds(-infinity));
    }
}

TEST_CASE("displacement shifts both bounds by the same amount", "[math][interval]") {
    const Interval range(1.0_f, 3.0_f);

    SECTION("with the interval on the left") {
        const Interval moved = range + 5.0_f;

        require_near(moved.min, 6.0_f);
        require_near(moved.max, 8.0_f);
        require_near(moved.size(), range.size());
    }
    SECTION("the operand order does not matter") {
        const Interval left = range + (-2.0_f);
        const Interval right = -2.0_f + range;

        require_near(left.min, right.min);
        require_near(left.max, right.max);
    }
}
