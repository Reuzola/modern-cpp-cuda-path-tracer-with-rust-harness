#include "pt/util/thread_pool.hpp"
#include <algorithm>
#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <cstddef>
#include <cstdint>
#include <latch>
#include <memory>
#include <numeric>
#include <stdexcept>
#include <thread>
#include <vector>

// Catch2 assertions are not thread-safe: tasks only record what happened, and
// every REQUIRE runs on the test thread after wait() has returned.
//
// Some cases can only fail by hanging: a pool that never lets the waiting
// thread run work deadlocks rather than reporting an error.

namespace {

// Recursive fork-join in the shape a parallel tree build takes: every level
// waits on its own group from inside a task.
[[nodiscard]] std::uint64_t parallel_sum(pt::ThreadPool& pool, std::uint64_t begin, std::uint64_t end) {
    constexpr std::uint64_t leaf_size = 64;
    if (end - begin <= leaf_size) {
        std::uint64_t sum = 0;
        for (std::uint64_t i = begin; i < end; ++i)
            sum += i;
        return sum;
    }

    const std::uint64_t mid = begin + ((end - begin) / 2);
    std::uint64_t left = 0;
    std::uint64_t right = 0;

    pt::TaskGroup group(pool);
    group.run([&pool, &left, begin, mid] { left = parallel_sum(pool, begin, mid); });
    group.run([&pool, &right, mid, end] { right = parallel_sum(pool, mid, end); });
    group.wait();
    return left + right;
}

} // namespace

TEST_CASE("ThreadPool starts exactly the requested number of workers", "[util][thread_pool]") {
    const unsigned requested = GENERATE(0U, 1U, 3U);
    const pt::ThreadPool pool(requested);

    REQUIRE(pool.worker_count() == requested);
}

TEST_CASE("ThreadPool's default leaves one hardware thread for the caller", "[util][thread_pool]") {
    const unsigned hardware = std::max(std::thread::hardware_concurrency(), 1U);

    REQUIRE(pt::ThreadPool::default_worker_count() + 1U == hardware);
}

TEST_CASE("TaskGroup::wait returns only after every task has run exactly once", "[util][thread_pool]") {
    const unsigned workers = GENERATE(0U, 1U, 4U);
    pt::ThreadPool pool(workers);

    // Plain ints, one slot per task: no two tasks share a location, and wait()
    // alone must make the workers' writes visible here. Under TSan, a missing
    // happens-before edge in the pool shows up as a race on these slots.
    constexpr std::size_t task_count = 1000;
    std::vector<int> runs(task_count, 0);

    pt::TaskGroup group(pool);
    for (std::size_t i = 0; i < task_count; ++i) {
        group.run([&runs, i] { ++runs[i]; });
    }
    group.wait();

    REQUIRE(std::ranges::all_of(runs, [](int count) { return count == 1; }));
}

TEST_CASE("A pool without workers runs tasks on the waiting thread in submission order", "[util][thread_pool]") {
    pt::ThreadPool pool(0);
    pt::TaskGroup group(pool);

    constexpr std::size_t task_count = 16;
    std::vector<std::size_t> order;
    std::vector<std::thread::id> threads;

    for (std::size_t i = 0; i < task_count; ++i) {
        group.run([&order, &threads, i] {
            order.push_back(i);
            threads.push_back(std::this_thread::get_id());
        });
    }

    // With no workers, wait() is the only thing that executes tasks.
    REQUIRE(order.empty());

    group.wait();

    std::vector<std::size_t> expected(task_count);
    std::iota(expected.begin(), expected.end(), std::size_t{0});
    REQUIRE(order == expected);

    const std::thread::id caller = std::this_thread::get_id();
    REQUIRE(std::ranges::all_of(threads, [caller](std::thread::id id) { return id == caller; }));
}

TEST_CASE("TaskGroup::wait runs pending tasks on the calling thread", "[util][thread_pool]") {
    pt::ThreadPool pool(1);
    pt::TaskGroup group(pool);

    std::latch worker_busy(1);
    std::latch release_worker(1);
    std::thread::id releaser;

    // Occupy the only worker until another task releases it.
    group.run([&worker_busy, &release_worker] {
        worker_busy.count_down();
        release_worker.wait();
    });
    worker_busy.wait();

    // Submitted once the worker is known to be busy: only the waiting thread can run it.
    group.run([&release_worker, &releaser] {
        releaser = std::this_thread::get_id();
        release_worker.count_down();
    });
    group.wait();

    REQUIRE(releaser == std::this_thread::get_id());
}

TEST_CASE("The waiting thread works alongside every worker", "[util][thread_pool]") {
    constexpr unsigned workers = 3;
    pt::ThreadPool pool(workers);
    pt::TaskGroup group(pool);

    // Opens only once workers + 1 tasks are blocked in it at the same time,
    // which takes every worker plus the waiting thread.
    std::latch all_running(workers + 1);
    for (unsigned i = 0; i <= workers; ++i) {
        group.run([&all_running] { all_running.arrive_and_wait(); });
    }
    group.wait();

    SUCCEED("workers + 1 tasks ran concurrently");
}

TEST_CASE("Tasks may submit to and wait on nested groups", "[util][thread_pool]") {
    const unsigned workers = GENERATE(0U, 1U, 4U);
    pt::ThreadPool pool(workers);

    constexpr int outer_tasks = 8;
    constexpr int inner_tasks = 8;
    std::atomic<int> completed{0};

    pt::TaskGroup outer(pool);
    for (int i = 0; i < outer_tasks; ++i) {
        outer.run([&pool, &completed] {
            pt::TaskGroup inner(pool);
            for (int j = 0; j < inner_tasks; ++j) {
                inner.run([&completed] { completed.fetch_add(1, std::memory_order_relaxed); });
            }
            inner.wait();
        });
    }
    outer.wait();

    REQUIRE(completed.load() == outer_tasks * inner_tasks);
}

TEST_CASE("Recursive fork-join completes on any worker count", "[util][thread_pool]") {
    const unsigned workers = GENERATE(0U, 1U, 4U);
    pt::ThreadPool pool(workers);

    constexpr std::uint64_t n = 1ULL << 16U;
    REQUIRE(parallel_sum(pool, 0, n) == n * (n - 1) / 2);
}

TEST_CASE("TaskGroup::wait rethrows a task's exception after the rest have run", "[util][thread_pool]") {
    const unsigned workers = GENERATE(0U, 1U, 4U);
    pt::ThreadPool pool(workers);
    pt::TaskGroup group(pool);

    constexpr std::size_t task_count = 16;
    constexpr std::size_t failing_task = 5;
    std::atomic<std::size_t> completed{0};

    for (std::size_t i = 0; i < task_count; ++i) {
        group.run([&completed, i] {
            if (i == failing_task) throw std::runtime_error("task failed");
            completed.fetch_add(1, std::memory_order_relaxed);
        });
    }

    REQUIRE_THROWS_AS(group.wait(), std::runtime_error);
    REQUIRE(completed.load() == task_count - 1);

    // The exception is handed over once; the group stays usable.
    REQUIRE_NOTHROW(group.wait());
    group.run([&completed] { completed.fetch_add(1, std::memory_order_relaxed); });
    REQUIRE_NOTHROW(group.wait());
    REQUIRE(completed.load() == task_count);
}

TEST_CASE("TaskGroup keeps the first exception thrown", "[util][thread_pool]") {
    // No workers: tasks run in submission order, so "first" is well defined.
    pt::ThreadPool pool(0);
    pt::TaskGroup group(pool);

    group.run([] { throw std::runtime_error("first"); });
    group.run([] { throw std::logic_error("second"); });

    REQUIRE_THROWS_AS(group.wait(), std::runtime_error);
}

TEST_CASE("TaskGroup's destructor waits for its tasks", "[util][thread_pool]") {
    pt::ThreadPool pool(2);

    constexpr std::size_t task_count = 64;
    std::vector<int> runs(task_count, 0);
    {
        pt::TaskGroup group(pool);
        for (std::size_t i = 0; i < task_count; ++i) {
            group.run([&runs, i] { runs[i] = 1; });
        }
    }

    REQUIRE(std::ranges::all_of(runs, [](int flag) { return flag == 1; }));
}

TEST_CASE("TaskGroup's destructor drops an exception nobody waited for", "[util][thread_pool]") {
    pt::ThreadPool pool(1);

    REQUIRE_NOTHROW([&pool] {
        pt::TaskGroup group(pool);
        group.run([] { throw std::runtime_error("never observed"); });
    }());
}

TEST_CASE("A group may be destroyed as soon as wait returns", "[util][thread_pool]") {
    pt::ThreadPool pool(4);

    // Heap-allocated so that a worker touching the group after its last
    // decrement is a heap-use-after-free, which asan-ubsan reports. The race
    // window is narrow; the repetition is what makes hitting it likely.
    constexpr int iterations = 2000;
    std::atomic<int> total{0};

    for (int i = 0; i < iterations; ++i) {
        auto group = std::make_unique<pt::TaskGroup>(pool);
        group->run([&total] { total.fetch_add(1, std::memory_order_relaxed); });
        group->wait();
        group.reset();
    }

    REQUIRE(total.load() == iterations);
}
