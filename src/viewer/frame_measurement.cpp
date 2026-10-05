#include "viewer/frame_measurement.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
#include <optional>
#include <ostream>
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

    const auto column = [&frames](double FrameTimes::* field) {
        std::vector<double> out;
        out.reserve(frames.size());

        std::ranges::transform(frames, std::back_inserter(out), field);
        return out;
    };

    PhaseSummary summary{};
    summary.frames = static_cast<int>(std::ssize(frames));

    const std::vector<double> frame_times = column(&FrameTimes::frame_ms);
    summary.frame_p50_ms = percentile(frame_times, 0.5);
    summary.frame_p95_ms = percentile(frame_times, 0.95);
    summary.frame_max_ms = percentile(frame_times, 1.0);

    summary.render_p50_ms = percentile(column(&FrameTimes::render_ms), 0.5);
    summary.display_p50_ms = percentile(column(&FrameTimes::display_ms), 0.5);
    summary.present_p50_ms = percentile(column(&FrameTimes::present_ms), 0.5);

    return summary;
}

[[nodiscard]] nlohmann::json to_json(const PhaseSummary& summary) {
    nlohmann::json j;
    j["frames"] = summary.frames;

    j["frame_ms"]["p50"] = summary.frame_p50_ms;
    j["frame_ms"]["p95"] = summary.frame_p95_ms;
    j["frame_ms"]["max"] = summary.frame_max_ms;

    j["render_ms_p50"] = summary.render_p50_ms;
    j["display_ms_p50"] = summary.display_p50_ms;
    j["present_ms_p50"] = summary.present_p50_ms;

    return j;
}

} // namespace

void FrameMeasurement::record(const FrameTimes& times, bool rendered_pass) {
    switch (phase_) {
    case Phase::warmup:
        --warmup_left_;
        if (warmup_left_ == 0) phase_ = Phase::moving;
        break;

    case Phase::moving:
        moving_.push_back(times);
        if (moving_.size() >= static_cast<std::size_t>(frames_per_phase_)) phase_ = Phase::still;
        break;

    case Phase::still:
        // A frame with no pass means the image converged; idle frames say nothing about rendering.
        if (!rendered_pass) {
            phase_ = Phase::done;
            break;
        }
        still_.push_back(times);
        if (still_.size() >= static_cast<std::size_t>(frames_per_phase_)) phase_ = Phase::done;
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

    out << j.dump() << '\n';
}

} // namespace pt
