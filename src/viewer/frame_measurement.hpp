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
    double frame_ms{};                // Whole loop iteration.
    double display_ms{};              // Tone map, byte conversion and texture upload.
    double present_ms{};              // Image and UI draw plus the buffer swap.
    bool new_image{};                 // A session image arrived this frame.
    std::optional<double> latency_ms; // Post-to-screen time of the edit this frame first shows.
};

struct PhaseSummary {
    int frames{};
    int images{};
    double frame_p50_ms{};
    double frame_p95_ms{};
    double frame_max_ms{};
    std::optional<double> display_p50_ms{}; // Over frames that showed a new image; the others do no display work.
    double present_p50_ms{};
    std::optional<double> latency_p50_ms; // Empty in a phase that posted no edits.
    std::optional<double> latency_p95_ms;
};

class FrameMeasurement final {
public:
    // Sized for one frame per image; faster frames grow the vectors geometrically, a few cheap copies over a whole run.
    explicit FrameMeasurement(int images_per_phase) : images_per_phase_(images_per_phase) {
        assert(images_per_phase > 0);
        moving_.reserve(static_cast<std::size_t>(images_per_phase_));
        still_.reserve(static_cast<std::size_t>(images_per_phase_));
    }

    [[nodiscard]] CameraInput scripted_input() const noexcept {
        if (phase_ == Phase::warmup || phase_ == Phase::moving) return CameraInput{.look_dx = look_dx_per_frame};
        return CameraInput{};
    }

    void record(const FrameTimes& times, bool converged);

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

    int images_per_phase_{};
    int warmup_left_{warmup_frames};
    Phase phase_{Phase::warmup};
    int moving_images_{};
    int still_images_{};
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
    double passes_per_second{};        // The still view's passes over its wall time.
};

void write_measurement(const MeasurementRecord& record, std::ostream& out);

} // namespace pt
