#include "pt/util/parallel_for.hpp"
#include "pt/util/thread_pool.hpp"
#include <algorithm>
#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <cstddef>
#include <latch>
#include <numeric>
#include <stdexcept>
#include <stop_token>
#include <thread>
#include <vector>

// Same discipline as the pool tests: bodies only record, every REQUIRE runs on
// the test thread after parallel_for has returned. Some cases can only fail by hanging.

TEST_CASE("parallel_for calls the body exactly once per index", "[util][parallel_for]") {
    const unsigned workers = GENERATE(0U, 1U, 4U);
    const std::size_t count = GENERATE(std::size_t{0}, std::size_t{1}, std::size_t{3}, std::size_t{1000});
    pt::ThreadPool pool(workers);

    // One plain int per index: under TSan a missing happens-before edge between
    // the bodies and the return of parallel_for shows up as a race here.
    std::vector<int> calls(count, 0);
    pt::parallel_for(pool, count, [&calls](std::size_t i) { ++calls[i]; });

    REQUIRE(std::ranges::all_of(calls, [](int n) { return n == 1; }));
}

TEST_CASE("parallel_for without workers runs on the caller in index order", "[util][parallel_for]") {
    pt::ThreadPool pool(0);

    constexpr std::size_t count = 64;
    std::vector<std::size_t> order;
    std::vector<std::thread::id> threads;

    pt::parallel_for(pool, count, [&order, &threads](std::size_t i) {
        order.push_back(i);
        threads.push_back(std::this_thread::get_id());
    });

    std::vector<std::size_t> expected(count);
    std::iota(expected.begin(), expected.end(), std::size_t{0});
    REQUIRE(order == expected);

    const std::thread::id caller = std::this_thread::get_id();
    REQUIRE(std::ranges::all_of(threads, [caller](std::thread::id id) { return id == caller; }));
}

TEST_CASE("parallel_for runs on every worker and the calling thread at once", "[util][parallel_for]") {
    constexpr unsigned workers = 3;
    pt::ThreadPool pool(workers);

    // Opens only when workers + 1 bodies are blocked in it together, which takes
    // every worker plus the calling thread.
    std::latch all_running(workers + 1);
    pt::parallel_for(pool, workers + 1, [&all_running](std::size_t) { all_running.arrive_and_wait(); });

    SUCCEED("workers + 1 bodies ran concurrently");
}

TEST_CASE("parallel_for hands a stalled thread's share to the others", "[util][parallel_for]") {
    pt::ThreadPool pool(1);

    // Index 0 cannot return until every other index has run. Any static split
    // gives the thread that holds index 0 more indices of its own, which it can
    // never reach: only dynamic hand-out lets the other thread take them all.
    constexpr std::size_t count = 64;
    std::latch rest_done(count - 1);

    pt::parallel_for(pool, count, [&rest_done](std::size_t i) {
        if (i == 0) {
            rest_done.wait();
        } else {
            rest_done.count_down();
        }
    });

    SUCCEED("every other index ran while index 0 was stalled");
}

TEST_CASE("parallel_for skips the remaining indices after a throw", "[util][parallel_for]") {
    // No workers: indices run in order, so "after the throw" is well defined.
    pt::ThreadPool pool(0);

    constexpr std::size_t count = 64;
    constexpr std::size_t failing = 5;
    std::vector<std::size_t> ran;

    REQUIRE_THROWS_AS(pt::parallel_for(pool, count,
                                       [&ran](std::size_t i) {
                                           if (i == failing) throw std::runtime_error("body failed");
                                           ran.push_back(i);
                                       }),
                      std::runtime_error);

    std::vector<std::size_t> expected(failing);
    std::iota(expected.begin(), expected.end(), std::size_t{0});
    REQUIRE(ran == expected);
}

TEST_CASE("parallel_for rethrows on any worker count and runs no index twice", "[util][parallel_for]") {
    const unsigned workers = GENERATE(0U, 1U, 4U);
    pt::ThreadPool pool(workers);

    constexpr std::size_t count = 1000;
    std::vector<std::atomic<int>> calls(count);

    REQUIRE_THROWS_AS(pt::parallel_for(pool, count,
                                       [&calls](std::size_t i) {
                                           calls[i].fetch_add(1, std::memory_order_relaxed);
                                           if (i == 0) throw std::runtime_error("body failed");
                                       }),
                      std::runtime_error);

    REQUIRE(std::ranges::all_of(calls, [](const std::atomic<int>& n) { return n.load() <= 1; }));
}

TEST_CASE("parallel_for runs nothing when stop is already requested", "[util][parallel_for]") {
    const unsigned workers = GENERATE(0U, 1U, 4U);
    pt::ThreadPool pool(workers);

    std::stop_source source;
    source.request_stop();

    std::atomic<int> calls{0};
    pt::parallel_for(pool, 1000, [&calls](std::size_t) { calls.fetch_add(1, std::memory_order_relaxed); }, source.get_token());

    // A caller that cancels between two loops - a viewer whose camera moved before
    // the next pass began - must pay nothing for the loop it no longer wants.
    REQUIRE(calls.load() == 0);
}

TEST_CASE("parallel_for hands out no index after stop is requested", "[util][parallel_for]") {
    // No workers: indices run in order, so "after the request" is well defined.
    pt::ThreadPool pool(0);
    std::stop_source source;

    constexpr std::size_t count = 64;
    constexpr std::size_t stopping = 5;
    std::vector<std::size_t> ran;

    pt::parallel_for(pool, count, [&ran, &source](std::size_t i) {
        ran.push_back(i);
        if (i == stopping) source.request_stop(); }, source.get_token());

    // Unlike a throw, the call that asked completes: cancelling is a normal outcome,
    // not a failure, so the index that requested it has run.
    std::vector<std::size_t> expected(stopping + 1);
    std::iota(expected.begin(), expected.end(), std::size_t{0});
    REQUIRE(ran == expected);
}

TEST_CASE("parallel_for starts at most one call per other participant after stop", "[util][parallel_for]") {
    const unsigned workers = GENERATE(1U, 4U);
    pt::ThreadPool pool(workers);
    std::stop_source source;

    constexpr std::size_t count = 10'000;
    std::vector<std::atomic<int>> calls(count);
    std::atomic<unsigned> started_after_stop{0};

    pt::parallel_for(pool, count, [&calls, &started_after_stop, &source](std::size_t i) {
        calls[i].fetch_add(1, std::memory_order_relaxed);
        if (source.stop_requested()) started_after_stop.fetch_add(1, std::memory_order_relaxed);
        if (i == 0) source.request_stop(); }, source.get_token());

    // Another participant can pass its check an instant before the request and
    // start one more call; its next check sees the stop. The requesting thread
    // starts none, so of workers + 1 participants at most `workers` begin a call
    // afterwards. The total is deliberately not bounded: if the requester is
    // descheduled before it asks, the others may legitimately run most indices.
    REQUIRE(started_after_stop.load() <= workers);
    REQUIRE(std::ranges::all_of(calls, [](const std::atomic<int>& n) { return n.load() <= 1; }));
}

TEST_CASE("parallel_for may be nested inside its own body", "[util][parallel_for]") {
    const unsigned workers = GENERATE(0U, 1U, 4U);
    pt::ThreadPool pool(workers);

    constexpr std::size_t outer = 8;
    constexpr std::size_t inner = 8;
    std::atomic<std::size_t> total{0};

    pt::parallel_for(pool, outer, [&pool, &total](std::size_t) {
        pt::parallel_for(pool, inner, [&total](std::size_t) { total.fetch_add(1, std::memory_order_relaxed); });
    });

    REQUIRE(total.load() == outer * inner);
}
