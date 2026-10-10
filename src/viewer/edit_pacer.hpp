#pragma once
#include "viewer/frame_measurement.hpp"
#include <cstdint>
#include <optional>

namespace pt {

// At most one edit in flight: the next is posted only once the last has reached the
// screen. Under continuous input every posted view then completes a pass and is
// shown, instead of each edit cancelling the one before it and freezing the image.
class EditPacer final {
public:
    [[nodiscard]] bool ready() const noexcept { return !in_flight_; }

    void posted(std::uint64_t generation) noexcept {
        in_flight_ = InFlight{.generation = generation, .posted = FrameClock::now()};
    }

    // Call on every frame that shows a new image, after the swap: it is what releases the next edit.
    [[nodiscard]] std::optional<double> displayed(std::uint64_t generation) noexcept {
        if (!in_flight_ || in_flight_->generation != generation) return std::nullopt;

        const double latency_ms = ms_since(in_flight_->posted);
        in_flight_.reset();
        return latency_ms;
    }

private:
    struct InFlight {
        std::uint64_t generation{};
        FrameClock::time_point posted;
    };

    std::optional<InFlight> in_flight_;
};

} // namespace pt
