#include "pt/math/interval.hpp"
#include <catch2/catch_test_macros.hpp>

TEST_CASE("engine headers compile and link into the test binary", "[smoke]") {
    STATIC_REQUIRE_FALSE(pt::Interval{}.contains(0.0));
    REQUIRE(pt::Interval::universe.contains(0.0));
}
