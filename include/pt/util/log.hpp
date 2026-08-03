#pragma once
#include <format>
#include <string_view>
#include <utility>

namespace pt {

// Order-sensitive: '>=' comparison with 'off' placed last blocks all records.
enum class LogLevel { info,
                      warning,
                      error,
                      off };

void set_log_level(LogLevel level) noexcept;

[[nodiscard]] LogLevel log_level() noexcept;

[[nodiscard]] bool should_log(LogLevel level) noexcept;

// Safe for inline temporaries (valid through full-expression); MUST NOT store the string_view.
void log_message(LogLevel level, std::string_view message);

// Intentional design: filtering precedes formatting across all wrappers.
template <typename... Args>
void log_info(std::format_string<Args...> fmt, Args&&... args) {
    if (!should_log(LogLevel::info)) return;
    log_message(LogLevel::info, std::format(fmt, std::forward<Args>(args)...));
}

template <typename... Args>
void log_warning(std::format_string<Args...> fmt, Args&&... args) {
    if (!should_log(LogLevel::warning)) return;
    log_message(LogLevel::warning, std::format(fmt, std::forward<Args>(args)...));
}

template <typename... Args>
void log_error(std::format_string<Args...> fmt, Args&&... args) {
    if (!should_log(LogLevel::error)) return;
    log_message(LogLevel::error, std::format(fmt, std::forward<Args>(args)...));
}

} // namespace pt
