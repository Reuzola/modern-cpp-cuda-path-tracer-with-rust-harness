#pragma once
#include <cstddef>

namespace pt {

// Keeps objects written by different threads off each other's cache lines.
// Hardcoded instead of std::hardware_destructive_interference_size, which
// clang 18 + libstdc++ lacks and whose value varies with -mtune (ABI hazard).
// 128, not 64: Intel prefetches line pairs and Apple cores use 128-byte lines.
inline constexpr std::size_t destructive_interference_size = 128;

} // namespace pt
