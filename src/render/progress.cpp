#include "pt/render/progress.hpp"
#include <format>
#include <iostream>

namespace pt {

void ConsoleProgressReporter::operator()(RenderProgress progress) noexcept {
    const int percent = static_cast<int>(progress.fraction() * 100.0);

    if (percent == last_percent_) return;
    last_percent_ = percent;

    std::clog << std::format("\rRendering: {:3}% ({} / {} tiles)", percent, progress.completed, progress.total) << std::flush;
    if (progress.completed == progress.total && progress.total > 0) std::clog << '\n';
}

} // namespace pt
