#include "benchmark.hpp"
#include "pt/math/scalar.hpp"
#include "pt/util/stats.hpp"
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <format>
#include <fstream>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
#include <optional>
#include <ostream>
#include <ratio>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>

// Forward-declare rusage so clangd doesn't demand internal glibc headers.
#include <sys/resource.h>
struct rusage;

#ifndef PT_BUILD_TYPE
#define PT_BUILD_TYPE "unknown"
#endif

#ifndef PT_GIT_REVISION
#define PT_GIT_REVISION "unknown"
#endif

namespace pt {

HostInfo detect_host() {
#if defined(__x86_64__)
    constexpr std::string_view arch = "x86_64";
#elif defined(__aarch64__)
    constexpr std::string_view arch = "aarch64";
#else
    constexpr std::string_view arch = "unknown";
#endif

    // AArch64 Linux has no "model name" line at all; the fallback is the normal path there, not a failure.
    std::string cpu_model = "unknown";
    std::ifstream cpuinfo("/proc/cpuinfo");
    std::string line;
    while (std::getline(cpuinfo, line)) {
        if (line.starts_with("model name")) {
            const std::size_t colon = line.find(':');
            if (colon != std::string::npos) {
                const std::size_t value_start = line.find_first_not_of(' ', colon + 1);
                // Only overwrite on a non-empty value, so "unknown" stays the
                // single spelling of an absent model.
                if (value_start != std::string::npos) cpu_model = line.substr(value_start);
            }
            break;
        }
    }

    // 0 means the count is unavailable; it is recorded as-is rather than guessed.
    const unsigned int logical_cores = std::thread::hardware_concurrency();

    return HostInfo{.cpu_model = cpu_model, .arch = std::string(arch), .logical_cores = logical_cores};
}

BuildInfo detect_build() {
#if defined(__clang__)
    const std::string compiler = __clang_version__;
#elif defined(__GNUC__)
    const std::string compiler = __VERSION__;
#else
    const std::string compiler = "unknown";
#endif

    const std::string build_type = PT_BUILD_TYPE;

    // The type is the source of truth, not the macro that selected it.
    const std::string scalar = std::is_same_v<Float, double> ? "double" : "float";

    const std::string revision = PT_GIT_REVISION;

    return BuildInfo{.compiler = compiler, .build_type = build_type, .scalar = scalar, .stats_enabled = stats_enabled, .revision = revision};
}

std::optional<std::uint64_t> peak_rss_bytes() noexcept {
#if defined(__linux__)
    rusage usage{};
    if (getrusage(RUSAGE_SELF, &usage) != 0) return std::nullopt;
    if (usage.ru_maxrss < 0) return std::nullopt;

    // Linux reports ru_maxrss in kibibytes; the record stores bytes.
    return static_cast<std::uint64_t>(usage.ru_maxrss) * 1024;
#else
    return std::nullopt;
#endif
}

std::string utc_timestamp() {
    return std::format("{:%FT%TZ}", std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now()));
}

void write_record(const BenchmarkRecord& record, std::ostream& out) {
    nlohmann::json j;

    j["schema_version"] = benchmark_schema_version;
    j["scene"] = record.scene;
    j["timestamp"] = record.timestamp;

    j["host"] = {
        {"cpu_model", record.host.cpu_model},
        {"arch", record.host.arch},
        {"logical_cores", record.host.logical_cores},
    };

    j["build"] = {
        {"compiler", record.build.compiler},
        {"build_type", record.build.build_type},
        {"scalar", record.build.scalar},
        {"stats_enabled", record.build.stats_enabled},
        {"revision", record.build.revision},
    };

    j["render"] = {
        {"width", record.image_width},
        {"height", record.image_height},
        {"samples_per_pixel", record.samples_per_pixel},
        {"max_depth", record.max_depth},
        {"seed", record.seed},
    };

    j["runtime"] = {{"threads", record.threads}};

    // min() has no answer for an empty range; null says "not measured" instead.
    const std::optional<double> fastest =
        record.render_seconds.empty() ? std::nullopt : std::optional(std::ranges::min(record.render_seconds));

    const std::uint64_t primary_rays = primary_ray_count(record.image_width, record.image_height, record.samples_per_pixel);

    // Derived from the same run as render_seconds_min, so the two cannot disagree.
    const std::optional<double> rate = fastest ? std::optional(static_cast<double>(primary_rays) / *fastest) : std::nullopt;

    j["timing"] = {
        {"runs", record.render_seconds.size()},
        {"render_seconds", record.render_seconds},
        {"render_seconds_min", fastest},
        {"primary_rays", primary_rays},
        {"primary_rays_per_second", rate},
    };

    j["memory"] = {{"peak_rss_bytes", record.peak_rss_bytes}};

    j["bvh"] = {
        {"trees", record.bvh.bvh_count},
        {"nodes", record.bvh.node_count},
        {"leaves", record.bvh.leaf_count},
        {"max_depth", record.bvh.max_depth},
        {"build_ms", std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(record.bvh.build_time).count()},
    };

    if (record.traversal) {
        j["traversal"] = {
            {"node_tests", record.traversal->node_tests},
            {"leaf_tests", record.traversal->leaf_tests},
            {"ray_queries", record.traversal->ray_queries},
        };
    } else {
        j["traversal"] = nullptr;
    }

    out << j.dump() << '\n';
}

} // namespace pt
