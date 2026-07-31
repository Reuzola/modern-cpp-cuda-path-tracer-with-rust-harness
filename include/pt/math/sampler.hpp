#pragma once
#include "pt/math/scalar.hpp"
#include <bit>
#include <cassert>
#include <cstdint>
#include <limits>

namespace pt {

[[nodiscard]] constexpr std::uint64_t hash_u64(std::uint64_t x) noexcept {
    x ^= x >> 30;
    x *= 0xBF58476D1CE4E5B9ULL;
    x ^= x >> 27;
    x *= 0x94D049BB133111EBULL;
    x ^= x >> 31;
    return x;
}

[[nodiscard]] constexpr std::uint64_t sampler_seed(std::uint64_t base_seed, std::uint64_t pixel_index, std::uint64_t sample_index) noexcept {
    constexpr std::uint64_t golden_ratio = 0x9E3779B97F4A7C15ULL;

    std::uint64_t hash = hash_u64(base_seed + golden_ratio);
    hash = hash_u64(hash ^ pixel_index);
    hash = hash_u64(hash ^ sample_index);
    return hash;
}

class Sampler { // PCG32
public:
    explicit constexpr Sampler(std::uint64_t seed) noexcept : state_(seed) {}

    [[nodiscard]] constexpr std::uint32_t next_uint32() noexcept {
        const std::uint64_t old_state = state_;
        state_ = old_state * multiplier + increment;

        const auto xorshifted = static_cast<std::uint32_t>(((old_state >> 18) ^ old_state) >> 27);
        const auto rotation = static_cast<int>(old_state >> 59);
        return std::rotr(xorshifted, rotation);
    }

    [[nodiscard]] constexpr std::uint32_t next_below(std::uint32_t bound) noexcept {
        assert(bound > 0);

        const std::uint64_t product = static_cast<std::uint64_t>(next_uint32()) * static_cast<std::uint64_t>(bound);
        return static_cast<std::uint32_t>(product >> 32);
    }

    [[nodiscard]] constexpr Float next_scalar() noexcept {
        if constexpr (std::numeric_limits<Float>::digits >= 32) {
            return static_cast<Float>(next_uint32()) * 0x1p-32_f;
        } else {
            return static_cast<Float>(next_uint32() >> 8) * 0x1p-24_f;
        }
    }

    [[nodiscard]] constexpr Float next_scalar(Float min, Float max) noexcept {
        return min + (max - min) * next_scalar();
    }

    [[nodiscard]] constexpr Float next_scalar_positive() noexcept {
        return 1.0_f - next_scalar();
    }

    [[nodiscard]] constexpr int next_int(int min, int max) noexcept {
        assert(min <= max);

        const auto span = static_cast<std::uint32_t>(max - min) + 1U;
        return min + static_cast<int>(next_below(span));
    }

private:
    static constexpr std::uint64_t multiplier = 6364136223846793005ULL;
    static constexpr std::uint64_t increment = 1442695040888963407ULL;

    std::uint64_t state_;
};

} // namespace pt
