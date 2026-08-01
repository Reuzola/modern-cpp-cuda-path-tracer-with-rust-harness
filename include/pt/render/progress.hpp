#pragma once
#include <functional>

namespace pt {

struct RenderProgress {
    int completed{};
    int total{};

    [[nodiscard]] constexpr double fraction() const noexcept {
        return (total <= 0) ? 0.0 : static_cast<double>(completed) / static_cast<double>(total);
    }
};

// Invoked total+1 times: once with completed=0 before rendering starts, then once after
// each completed unit, so the final call always has completed == total.
// The unit is defined by the renderer; it is currently one tile.
// RenderProgress is only 8 bytes. No need to use reference.
using ProgressCallback = std::function<void(RenderProgress)>;

class ConsoleProgressReporter final {
public:
    void operator()(RenderProgress progress) noexcept;

private:
    int last_percent_{-1};
};

} // namespace pt
