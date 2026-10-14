#include "viewer/frame_measurement.hpp"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
#include <optional>
#include <ostream>
#include <ratio>
#include <span>
#include <vector>

namespace pt {

namespace {

[[nodiscard]] double percentile(std::vector<double> values, double fraction) {
    assert(!values.empty());
    std::ranges::sort(values);

    // Nearest-rank: always an observed value, never an interpolation between two.
    std::size_t index = static_cast<std::size_t>(std::ceil(fraction * static_cast<double>(values.size())));
    if (index == 0) index = 1;
    --index;

    return values[index];
}

[[nodiscard]] PhaseSummary summarize(std::span<const FrameTimes> frames) {
    assert(!frames.empty());

    const auto column = [&frames](double FrameTimes::*field) {
        std::vector<double> out;
        out.reserve(frames.size());

        std::ranges::transform(frames, std::back_inserter(out), field);
        return out;
    };

    PhaseSummary summary{};
    summary.frames = static_cast<int>(std::ssize(frames));
    summary.images = static_cast<int>(std::ranges::count_if(frames, &FrameTimes::new_image));

    const std::vector<double> frame_times = column(&FrameTimes::frame_ms);
    summary.frame_p50_ms = percentile(frame_times, 0.5);
    summary.frame_p95_ms = percentile(frame_times, 0.95);
    summary.frame_max_ms = percentile(frame_times, 1.0);

    std::vector<double> displays;
    for (const FrameTimes& t : frames) {
        if (t.new_image) displays.push_back(t.display_ms);
    }

    if (!displays.empty()) {
        summary.display_p50_ms = percentile(displays, 0.5);
    }

    summary.present_p50_ms = percentile(column(&FrameTimes::present_ms), 0.5);

    std::vector<double> latencies;
    for (const FrameTimes& t : frames) {
        if (t.latency_ms) latencies.push_back(*t.latency_ms);
    }

    if (!latencies.empty()) {
        summary.latency_p50_ms = percentile(latencies, 0.5);
        summary.latency_p95_ms = percentile(latencies, 0.95);
    }

    const auto ms_between = [](FrameClock::time_point from, FrameClock::time_point to) {
        return std::chrono::duration<double, std::milli>(to - from).count();
    };

    std::vector<double> frame_intervals;
    for (std::size_t i = 1; i < frames.size(); ++i) {
        frame_intervals.push_back(ms_between(frames[i - 1].presented, frames[i].presented));
    }
    if (!frame_intervals.empty()) summary.frame_interval_p50_ms = percentile(frame_intervals, 0.5);

    // Only within the phase: the interval into its first image straddles the phase change.
    std::vector<double> image_intervals;
    std::optional<FrameClock::time_point> first_image;
    std::optional<FrameClock::time_point> last_image;
    for (const FrameTimes& t : frames) {
        if (!t.new_image) continue;
        if (last_image) image_intervals.push_back(ms_between(*last_image, t.presented));
        if (!first_image) first_image = t.presented;
        last_image = t.presented;
    }

    if (!image_intervals.empty()) {
        summary.image_interval_p50_ms = percentile(image_intervals, 0.5);
        summary.image_interval_p95_ms = percentile(image_intervals, 0.95);

        const double span_ms = ms_between(*first_image, *last_image);
        if (span_ms > 0.0) summary.images_per_second = 1000.0 * static_cast<double>(image_intervals.size()) / span_ms;
    }

    return summary;
}

[[nodiscard]] nlohmann::json to_json(const PhaseSummary& summary) {
    nlohmann::json j;
    j["frames"] = summary.frames;
    j["images"] = summary.images;

    j["frame_ms"]["p50"] = summary.frame_p50_ms;
    j["frame_ms"]["p95"] = summary.frame_p95_ms;
    j["frame_ms"]["max"] = summary.frame_max_ms;

    if (summary.display_p50_ms) {
        j["display_ms_p50"] = *summary.display_p50_ms;
    } else {
        j["display_ms_p50"] = nullptr;
    }

    j["present_ms_p50"] = summary.present_p50_ms;

    if (summary.latency_p50_ms && summary.latency_p95_ms) {
        j["latency_ms"]["p50"] = *summary.latency_p50_ms;
        j["latency_ms"]["p95"] = *summary.latency_p95_ms;
    } else {
        j["latency_ms"] = nullptr;
    }

    const auto optional = [](const std::optional<double>& value) {
        return value ? nlohmann::json(*value) : nlohmann::json(nullptr);
    };

    j["frame_interval_ms_p50"] = optional(summary.frame_interval_p50_ms);

    if (summary.image_interval_p50_ms && summary.image_interval_p95_ms) {
        j["image_interval_ms"]["p50"] = *summary.image_interval_p50_ms;
        j["image_interval_ms"]["p95"] = *summary.image_interval_p95_ms;
    } else {
        j["image_interval_ms"] = nullptr;
    }
    j["images_per_second"] = optional(summary.images_per_second);

    return j;
}

} // namespace

void FrameMeasurement::record(const FrameTimes& times, bool converged) {
    switch (phase_) {
    case Phase::warmup:
        if (times.new_image && --warmup_left_ == 0) phase_ = Phase::moving;
        break;

    case Phase::moving:
        moving_.push_back(times);
        if (times.new_image) ++moving_images_;
        if (moving_images_ >= images_per_phase_) phase_ = Phase::still;
        break;

    case Phase::still:
        still_.push_back(times);
        if (times.new_image) ++still_images_;

        // Once the image has converged, further frames only redraw it.
        if (converged || still_images_ >= images_per_phase_) phase_ = Phase::done;
        break;

    case Phase::done: break;
    }
}

PhaseSummary FrameMeasurement::moving() const {
    assert(done());
    return summarize(moving_);
}

std::optional<PhaseSummary> FrameMeasurement::still() const {
    assert(done());
    if (still_.empty()) return std::nullopt;
    return summarize(still_);
}

void write_measurement(const MeasurementRecord& record, std::ostream& out) {
    nlohmann::json j;
    j["scene"] = record.scene;
    j["width"] = record.width;
    j["height"] = record.height;
    j["threads"] = record.threads;
    j["moving"] = to_json(record.moving);
    j["still"] = record.still ? to_json(*record.still) : nlohmann::json(nullptr);
    j["passes_per_second"] = record.passes_per_second;

    out << j.dump() << '\n';
}

} // namespace pt
