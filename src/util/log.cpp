#include "pt/util/log.hpp"
#include <iostream>
#include <ostream>
#include <string_view>

namespace pt {

namespace {

// not thread safe yet. Only below 3 func should read/write.
constinit LogLevel current_level = LogLevel::info;

[[nodiscard]] std::string_view level_prefix(LogLevel level) noexcept {
    switch (level) {
    case LogLevel::info:
        return "";
    case LogLevel::warning:
        return "warning: ";
    case LogLevel::error:
        return "error: ";
    case LogLevel::off:
        return "";
    }
    return "";
}

} // namespace

void set_log_level(LogLevel level) noexcept { current_level = level; }

LogLevel log_level() noexcept { return current_level; }

bool should_log(LogLevel level) noexcept {
    return level >= current_level;
}

void log_message(LogLevel level, std::string_view message) {
    std::clog << level_prefix(level) << message << '\n'
              << std::flush;
}

} // namespace pt
