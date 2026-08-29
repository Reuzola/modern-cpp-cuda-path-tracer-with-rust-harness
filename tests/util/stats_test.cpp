#include "pt/util/stats.hpp"
#include <catch2/catch_test_macros.hpp>

// The counters are thread_local by design (D56's host-only diagnostic). That
// property is not exercised here: it would pull a threading dependency into the
// test target, and the merge hook the parallel renderer needs will own it.

TEST_CASE("stats_enabled mirrors the build option", "[util][stats]") {
    // The definition is PUBLIC on pathtracer_core, so the test binary sees the
    // same value the engine was compiled with. A mismatch here would mean the
    // engine and its consumers disagree about the layout of an inline function.
#ifdef PT_STATS
    STATIC_REQUIRE(pt::stats_enabled);
#else
    STATIC_REQUIRE_FALSE(pt::stats_enabled);
#endif
}

TEST_CASE("the counters are independent, and compile out when disabled", "[util][stats]") {
    // Shared mutable state: a bvh case may have run earlier in this process.
    pt::traversal_stats = pt::TraversalStats{};

    pt::count_node_test();
    pt::count_node_test();
    pt::count_leaf_test();
    pt::count_ray_query();
    pt::count_ray_query();
    pt::count_ray_query();

    const pt::TraversalStats& stats = pt::traversal_stats;

    if constexpr (pt::stats_enabled) {
        REQUIRE(stats.node_tests == 2);
        REQUIRE(stats.leaf_tests == 1);
        REQUIRE(stats.ray_queries == 3);
    } else {
        // Not merely "unused": the increments are gone. The traversal loop in a
        // default build pays nothing for instrumentation it did not ask for.
        REQUIRE(stats.node_tests == 0);
        REQUIRE(stats.leaf_tests == 0);
        REQUIRE(stats.ray_queries == 0);
    }

    // Leave the shared state as it was found.
    pt::traversal_stats = pt::TraversalStats{};
}
