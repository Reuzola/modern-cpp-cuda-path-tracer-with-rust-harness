#pragma once
#include "pt/util/thread_pool.hpp"
#include <algorithm>
#include <atomic>
#include <concepts>
#include <cstddef>

namespace pt {

// Calls body(i) for every i in [0, count), spread over the pool's workers and the
// calling thread; returns once every call has returned. Indices come one at a time
// from a shared counter, so faster threads take more and uneven costs balance out.
// body runs on several threads at once and must be thread-safe. With no workers,
// calls run on the caller in index order. If a call throws, indices not yet handed
// out are skipped and the first exception is rethrown after running calls finish.
template <typename F>
    requires std::invocable<F&, std::size_t>
void parallel_for(ThreadPool& pool, std::size_t count, F&& body) {
    if (count == 0) return;

    const std::size_t participants = std::min(count, static_cast<std::size_t>(pool.worker_count()) + 1);

    // Declared before the group: the group's destructor waits for the tasks that use it.
    std::atomic<std::size_t> next{0};

    const auto claim = [&next, &body, count] {
        // Relaxed: the counter only hands out indices; results reach the caller through wait().
        for (std::size_t i = next.fetch_add(1, std::memory_order_relaxed); i < count; i = next.fetch_add(1, std::memory_order_relaxed)) {
            try {
                body(i);
            } catch (...) {
                // Stop handing out indices; the group keeps the exception for wait().
                next.store(count, std::memory_order_relaxed);
                throw;
            }
        }
    };

    TaskGroup group(pool);

    // Every participant is a task, the caller's included (wait() runs one): one exception path.
    for (std::size_t j = 0; j < participants; ++j) {
        group.run(claim);
    }
    group.wait();
}

} // namespace pt
