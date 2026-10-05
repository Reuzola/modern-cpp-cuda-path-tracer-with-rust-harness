#pragma once
#include "pt/math/scalar.hpp"
#include "viewer/camera_controller.hpp"
#include <cassert>
#include <chrono>
#include <cstddef>
#include <iosfwd>
#include <optional>
#include <ratio>
#include <string>
#include <vector>

namespace pt {

using FrameClock = std::chrono::steady_clock;

[[nodiscard]] inline double ms_since(FrameClock::time_point start) noexcept {
    return std::chrono::duration<double, std::milli>(FrameClock::now() - start).count();
}

struct FrameTimes {
    double frame_ms{};   // Whole loop iteration.
    double render_ms{};  // render_pass() alone; 0 when no pass ran.
    double display_ms{}; // Resolve, tone map, byte conversion and texture upload.
    double present_ms{}; // Image and UI draw plus the buffer swap.
};

struct PhaseSummary {
    int frames{};
    double frame_p50_ms{};
    double frame_p95_ms{};
    double frame_max_ms{};
    double render_p50_ms{};
    double display_p50_ms{};
    double present_p50_ms{};
};

class FrameMeasurement final {
public:
    // Reserved up front so recording never allocates inside a measured frame.
    explicit FrameMeasurement(int frames_per_phase) : frames_per_phase_(frames_per_phase) {
        assert(frames_per_phase > 0);
        moving_.reserve(static_cast<std::size_t>(frames_per_phase_));
        still_.reserve(static_cast<std::size_t>(frames_per_phase_));
    }

    [[nodiscard]] CameraInput scripted_input() const noexcept {
        if (phase_ == Phase::warmup || phase_ == Phase::moving) return CameraInput{.look_dx = look_dx_per_frame};
        return CameraInput{};
    }

    void record(const FrameTimes& times, bool rendered_pass);

    [[nodiscard]] bool done() const noexcept { return phase_ == Phase::done; }

    [[nodiscard]] PhaseSummary moving() const;

    [[nodiscard]] std::optional<PhaseSummary> still() const;

private:
    // clang-format off
    enum class Phase { warmup, moving, still, done };
    // clang-format on

    // The first frames pay for shader compilation and first-touch page faults.
    static constexpr int warmup_frames = 10;

    // Yaw only, a fixed step per frame: the camera never walks into geometry and every run sees the same views.
    static constexpr Float look_dx_per_frame = 8.0_f;

    int frames_per_phase_{};
    int warmup_left_{warmup_frames};
    Phase phase_{Phase::warmup};
    std::vector<FrameTimes> moving_;
    std::vector<FrameTimes> still_;
};

struct MeasurementRecord {
    std::string scene;
    int width{};
    int height{};
    int threads{};
    PhaseSummary moving;
    std::optional<PhaseSummary> still; // Empty when the image converged before the first still frame (a 1 spp target).
};

void write_measurement(const MeasurementRecord& record, std::ostream& out);

} // namespace pt
