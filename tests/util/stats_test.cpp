#include "pt/util/stats.hpp"
#include "pt/util/thread_pool.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <thread>
#include <vector>

// The counters are process-wide mutable state: any case that traversed a tree
// may have counted before these run. Every case therefore starts from a reset,
// and touches the counters only through the snapshot API, so the storage behind
// that API can change without these cases noticing. Cases that count on other
// threads read only after joining them or waiting on their group: the snapshot
// is defined only once every counting thread has been synchronized with.

namespace {

// What a counter reads after `n` increments: `n` in an instrumented build, zero
// where the increments are compiled out. One assertion states both contracts.
[[nodiscard]] constexpr std::uint64_t counted(std::uint64_t n) noexcept { return pt::stats_enabled ? n : 0; }

void count(std::uint64_t node_tests, std::uint64_t leaf_tests, std::uint64_t ray_queries) {
    for (std::uint64_t i = 0; i < node_tests; ++i)
        pt::count_node_test();
    for (std::uint64_t i = 0; i < leaf_tests; ++i)
        pt::count_leaf_test();
    for (std::uint64_t i = 0; i < ray_queries; ++i)
        pt::count_ray_query();
}

void require_counts(const pt::TraversalStats& stats, std::uint64_t node_tests, std::uint64_t leaf_tests,
                    std::uint64_t ray_queries) {
    REQUIRE(stats.node_tests == counted(node_tests));
    REQUIRE(stats.leaf_tests == counted(leaf_tests));
    REQUIRE(stats.ray_queries == counted(ray_queries));
}

} // namespace

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

TEST_CASE("traversal stats add field by field", "[util][stats]") {
    // A different magnitude per field: a sum that crossed two fields over would
    // land on a value no correct sum can produce. Evaluated at compile time, so
    // it holds in every build, instrumented or not.
    constexpr pt::TraversalStats total = [] {
        pt::TraversalStats sum{.node_tests = 1, .leaf_tests = 2, .ray_queries = 3};
        sum += pt::TraversalStats{.node_tests = 10, .leaf_tests = 200, .ray_queries = 3000};
        return sum;
    }();

    STATIC_REQUIRE(total == pt::TraversalStats{.node_tests = 11, .leaf_tests = 202, .ray_queries = 3003});
}

TEST_CASE("counts from every thread reach the snapshot, the caller's included", "[util][stats]") {
    pt::reset_traversal_stats();

    // The calling thread runs tasks while it waits, so its own slot is part of
    // every render's total, not just the workers'.
    count(1, 2, 3);

    {
        std::vector<std::jthread> threads;
        for (std::uint64_t t = 0; t < 4; ++t) {
            // A different amount per thread: a slot dropped from the sum, or
            // summed twice, moves the total off the expected value.
            const std::uint64_t scale = std::uint64_t{1} << t;
            threads.emplace_back([scale] { count(100 * scale, 10 * scale, scale); });
        }
    } // Every jthread joins here, and its thread has exited.

    // The threads are gone, their counts are not: a slot belongs to the
    // registry, not to the thread that wrote it.
    require_counts(pt::traversal_snapshot(), 1 + 1500, 2 + 150, 3 + 15);

    pt::reset_traversal_stats();
}

TEST_CASE("pool tasks' counts are complete once their group has waited", "[util][stats]") {
    // The renderer's situation: workers stay alive and idle, never joined. What
    // makes their counts readable is TaskGroup::wait(), not thread exit.
    pt::ThreadPool pool(3);
    pt::reset_traversal_stats();

    {
        pt::TaskGroup group(pool);
        for (int i = 0; i < 64; ++i) {
            group.run([] { count(50, 5, 1); });
        }
        group.wait();
    }

    require_counts(pt::traversal_snapshot(), static_cast<std::uint64_t>(64) * 50, static_cast<std::uint64_t>(64) * 5, 64);

    pt::reset_traversal_stats();
}

TEST_CASE("a reset clears every thread's slot and leaves it in use", "[util][stats]") {
    pt::ThreadPool pool(3);
    pt::reset_traversal_stats();

    const auto run_batch = [&pool](std::uint64_t node_tests) {
        pt::TaskGroup group(pool);
        for (int i = 0; i < 64; ++i) {
            group.run([node_tests] { count(node_tests, 0, 0); });
        }
        group.wait();
    };

    // First batch: the workers register their slots.
    run_batch(50);
    pt::reset_traversal_stats();

    // The reset reached the idle workers' slots, not only the caller's.
    require_counts(pt::traversal_snapshot(), 0, 0, 0);

    // The same workers count again into the same slots. A reset that released
    // the slots instead of zeroing them would leave each worker writing through
    // a dangling pointer, and this batch would vanish from the total.
    run_batch(7);
    require_counts(pt::traversal_snapshot(), static_cast<std::uint64_t>(64) * 7, 0, 0);

    pt::reset_traversal_stats();
}
