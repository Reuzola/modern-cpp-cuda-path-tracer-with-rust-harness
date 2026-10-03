#pragma once
#include "pt/util/cache_line.hpp"
#include <cstdint>

namespace pt {

#ifdef PT_STATS
inline constexpr bool stats_enabled = true;
#else
inline constexpr bool stats_enabled = false;
#endif

struct TraversalStats {
    std::uint64_t node_tests{};
    std::uint64_t leaf_tests{};
    std::uint64_t ray_queries{};

    constexpr TraversalStats& operator+=(const TraversalStats& other) noexcept {
        node_tests += other.node_tests;
        leaf_tests += other.leaf_tests;
        ray_queries += other.ray_queries;
        return *this;
    }

    friend constexpr bool operator==(const TraversalStats&, const TraversalStats&) noexcept = default;
};

// Every counter must be summed in operator+= and written by the benchmark
// record; this fails until both know about a new one.
static_assert(sizeof(TraversalStats) == 3 * sizeof(std::uint64_t),
              "a new counter must be added to operator+= and to the benchmark record");

namespace detail {

// One thread's counters, alone on their cache lines: a neighbour's slot sharing
// a line would bounce it between cores on every node test.
struct alignas(destructive_interference_size) TraversalSlot {
    TraversalStats stats;
};

static_assert(alignof(TraversalSlot) == destructive_interference_size);

// The calling thread's slot, registered on its first count. Trivial and
// constant-initialized, so access compiles to a plain TLS load with no guard.
inline constinit thread_local TraversalSlot* traversal_slot = nullptr;

// Slow path, once per thread: allocates a slot, hands it to the registry and
// points traversal_slot at it. noexcept: a 128-byte allocation failing in a
// measurement build is not worth a recovery path, so it terminates.
[[nodiscard]] TraversalStats& register_traversal_slot() noexcept;

[[nodiscard]] inline TraversalStats& local_traversal_stats() noexcept {
    if (traversal_slot == nullptr) [[unlikely]] {
        return register_traversal_slot();
    }
    return traversal_slot->stats;
}

} // namespace detail

inline void count_node_test() noexcept {
    if constexpr (stats_enabled) ++detail::local_traversal_stats().node_tests;
}

inline void count_leaf_test() noexcept {
    if constexpr (stats_enabled) ++detail::local_traversal_stats().leaf_tests;
}

inline void count_ray_query() noexcept {
    if constexpr (stats_enabled) ++detail::local_traversal_stats().ray_queries;
}

// The single read and reset point: sums, or zeroes, the slot of every thread
// that has ever counted, live or exited. Call only when no thread is counting
// and after synchronizing with every thread that did (TaskGroup::wait, join);
// Renderer::render and render_pass return at such a point. Reading during a
// render is a data race by design: the total would be partial and
// nondeterministic, and plain fields let TSan say so.
[[nodiscard]] TraversalStats traversal_snapshot() noexcept;
void reset_traversal_stats() noexcept;

} // namespace pt
