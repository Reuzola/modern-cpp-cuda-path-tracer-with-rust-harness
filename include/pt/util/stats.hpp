#pragma once

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
};

inline constinit thread_local TraversalStats traversal_stats{};

inline void count_node_test() noexcept {
    if constexpr (stats_enabled) ++traversal_stats.node_tests;
}

inline void count_leaf_test() noexcept {
    if constexpr (stats_enabled) ++traversal_stats.leaf_tests;
}

inline void count_ray_query() noexcept {
    if constexpr (stats_enabled) ++traversal_stats.ray_queries;
}

} // namespace pt
