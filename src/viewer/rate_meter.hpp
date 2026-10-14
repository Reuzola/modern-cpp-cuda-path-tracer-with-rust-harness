#pragma once
#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <optional>
#include <ratio>

namespace pt {

struct Rate {
    double interval_ms{};
    double per_second{};
};

// How often an event happens, over a trailing window of wall time: one meter
// counts the frames the loop presents, another the frames that carry a new image.
// The reported interval is never shorter than the age of the latest event. An
// average over the last N events (what ImGui's io.Framerate is) keeps reporting
// the rate events arrived at after they stop; here an image that stays on screen
// for two seconds reads as 0.5 per second, which is what the eye sees.
class RateMeter final {
public:
    using Clock = std::chrono::steady_clock;

    explicit RateMeter(Clock::duration window) noexcept : window_(window) { assert(window > Clock::duration::zero()); }

    void tick(Clock::time_point t) noexcept {
        assert(count_ == 0 || t >= latest());
        ticks_[next_] = t;
        next_ = (next_ + 1) % capacity;
        count_ = std::min(count_ + 1, capacity);
    }

    // Empty until two events have happened: one event alone has no interval.
    [[nodiscard]] std::optional<Rate> rate(Clock::time_point now) const noexcept {
        if (count_ < 2) return std::nullopt;

        const Clock::time_point last = latest();
        const Clock::time_point window_start = now - window_;

        // The latest event and the one before it always count, so a rate slower
        // than the window still has an interval; older ones only inside the window.
        std::size_t used = 2;
        Clock::time_point oldest = at(1);
        while (used < count_ && at(used) >= window_start) {
            oldest = at(used);
            ++used;
        }

        const double mean_ms = std::chrono::duration<double, std::milli>(last - oldest).count() / static_cast<double>(used - 1);
        const double age_ms = std::chrono::duration<double, std::milli>(now - last).count();
        const double interval_ms = std::max(mean_ms, age_ms);
        if (interval_ms <= 0.0) return std::nullopt; // Two events on one clock tick: no rate to speak of yet.

        return Rate{.interval_ms = interval_ms, .per_second = 1000.0 / interval_ms};
    }

private:
    // A second of frames at 256 Hz; a faster loop measures over a shorter span, same mean.
    static constexpr std::size_t capacity = 256;

    std::array<Clock::time_point, capacity> ticks_{};
    std::size_t next_{};
    std::size_t count_{};
    Clock::duration window_{};

    // 0 is the latest event, 1 the one before it.
    [[nodiscard]] Clock::time_point at(std::size_t back) const noexcept {
        assert(back < count_);
        return ticks_[(next_ + capacity - 1 - back) % capacity];
    }

    [[nodiscard]] Clock::time_point latest() const noexcept { return at(0); }
};

} // namespace pt
