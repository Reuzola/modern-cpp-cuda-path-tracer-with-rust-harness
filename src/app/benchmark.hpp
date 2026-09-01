#pragma once
#include "pt/geometry/bvh.hpp"
#include "pt/util/stats.hpp"
#include <cstdint>
#include <iosfwd>
#include <optional>
#include <string>
#include <vector>

namespace pt {

// Bumped on any change to the record's shape or to the meaning of a field.
inline constexpr int benchmark_schema_version = 2;

struct HostInfo {
    std::string cpu_model;
    std::string arch;
    unsigned int logical_cores{};
};

struct BuildInfo {
    std::string compiler;
    std::string build_type;
    std::string scalar; // "double" or "float"
    bool stats_enabled{};
    std::string revision; // Short commit hash from CMake configure time. Blind to uncommitted edits.
};

// One primary ray per sample. spp must be the count the renderer used, not the one requested.
[[nodiscard]] constexpr std::uint64_t primary_ray_count(int width, int height, int samples_per_pixel) noexcept {
    return static_cast<std::uint64_t>(width) * static_cast<std::uint64_t>(height) * static_cast<std::uint64_t>(samples_per_pixel);
}

struct BenchmarkRecord {
    std::string scene;
    std::string timestamp;
    HostInfo host;
    BuildInfo build;
    int image_width{};
    int image_height{};
    int samples_per_pixel{};
    int max_depth{};
    std::uint64_t seed{};
    int threads{1};
    std::vector<double> render_seconds;
    std::optional<std::uint64_t> peak_rss_bytes;
    BvhStats bvh{};
    std::optional<TraversalStats> traversal;
};

[[nodiscard]] HostInfo detect_host();

[[nodiscard]] BuildInfo detect_build();

// Process-wide high-water mark: includes scene load and
// BVH build, which the timed runs exclude. Not resettable.
[[nodiscard]] std::optional<std::uint64_t> peak_rss_bytes() noexcept;

// Captured when the measurement is taken, not when the record is written.
[[nodiscard]] std::string utc_timestamp();

void write_record(const BenchmarkRecord& record, std::ostream& out);

} // namespace pt
