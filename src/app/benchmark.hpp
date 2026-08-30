#pragma once
#include "pt/geometry/bvh.hpp"
#include "pt/util/stats.hpp"
#include <cstdint>
#include <optional>
#include <iosfwd>
#include <string>
#include <vector>

namespace pt {

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
};

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
    std::vector<double> render_seconds;
    BvhStats bvh{};
    std::optional<TraversalStats> traversal;
};

[[nodiscard]] HostInfo detect_host();

[[nodiscard]] BuildInfo detect_build();

// Captured when the measurement is taken, not when the record is written.
[[nodiscard]] std::string utc_timestamp();

void write_record(const BenchmarkRecord& record, std::ostream& out);

} // namespace pt
