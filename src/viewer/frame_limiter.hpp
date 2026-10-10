#pragma once
#include "viewer/frame_measurement.hpp"
#include <chrono>
#include <thread>

namespace pt {

// Caps the loop at a frame rate by sleeping out the rest of each frame's budget.
// Vsync would do this, but swap intervals are not honoured everywhere (WSLg among
// them), and an uncapped loop spends a core - and the GL driver's threads - on
// frames no display can show, taking that time from the render threads.
class FrameLimiter final {
public:
    explicit FrameLimiter(int max_fps)
        : period_(max_fps > 0
                      ? std::chrono::duration_cast<FrameClock::duration>(std::chrono::duration<double>(1.0 / static_cast<double>(max_fps)))
                      : FrameClock::duration::zero()),
          next_(FrameClock::now() + period_) {}

    void wait() {
        if (period_ == FrameClock::duration::zero()) return;

        const auto now = FrameClock::now();
        if (now < next_) {
            std::this_thread::sleep_until(next_);
            next_ += period_;
        } else {
            // Late: start a fresh budget instead of rushing short frames to catch up.
            next_ = now + period_;
        }
    }

private:
    FrameClock::duration period_{};
    FrameClock::time_point next_{};
};

} // namespace pt
