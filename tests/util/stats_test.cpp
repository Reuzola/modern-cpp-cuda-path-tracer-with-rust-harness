#include "pt/util/stats.hpp"
#include <catch2/catch_test_macros.hpp>

// The counters are process-wide mutable state: any case that traversed a tree
// may have counted before these run. Every case therefore starts from a reset,
// and touches the counters only through the snapshot API, so the storage behind
// that API can change without these cases noticing.

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
    pt::reset_traversal_stats();

    pt::count_node_test();
    pt::count_node_test();
    pt::count_leaf_test();
    pt::count_ray_query();
    pt::count_ray_query();
    pt::count_ray_query();

    const pt::TraversalStats stats = pt::traversal_snapshot();

    if constexpr (pt::stats_enabled) {
        // A different count per counter: an increment wired to the wrong field
        // shows up as two wrong numbers instead of hiding behind an equal one.
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

    pt::reset_traversal_stats();
}

TEST_CASE("a reset zeroes every counter", "[util][stats]") {
    pt::count_node_test();
    pt::count_leaf_test();
    pt::count_ray_query();

    pt::reset_traversal_stats();
    const pt::TraversalStats stats = pt::traversal_snapshot();

    // Every field, not one: the benchmark resets between timed runs, and a field
    // the reset skipped would carry the previous run into the next record.
    REQUIRE(stats.node_tests == 0);
    REQUIRE(stats.leaf_tests == 0);
    REQUIRE(stats.ray_queries == 0);
}
