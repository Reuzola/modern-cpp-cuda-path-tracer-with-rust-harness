#pragma once
#include "pt/util/log.hpp"

namespace pt_test {

/// Suppresses engine log output for a scope and restores the previous level on
/// exit. Used where the subject is *expected* to log - a missing image file is a
/// warning, not a load error - so that a passing run stays quiet and the only
/// thing on the console is a real failure.
class LogSilencer {
public:
    LogSilencer() : previous_(pt::log_level()) { pt::set_log_level(pt::LogLevel::off); }

    LogSilencer(const LogSilencer&) = delete;
    LogSilencer& operator=(const LogSilencer&) = delete;

    ~LogSilencer() { pt::set_log_level(previous_); }

private:
    pt::LogLevel previous_;
};

} // namespace pt_test
