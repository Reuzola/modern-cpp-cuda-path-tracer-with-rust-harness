#include "pt/util/log.hpp"
#include "support/clog_capture.hpp"
#include <catch2/catch_test_macros.hpp>
#include <format>
#include <string>

// Outside the anonymous namespace: the std::formatter specialisation below names
// this type, and a specialisation of a std template is cleaner against a type
// with external linkage.
namespace pt_test_log {

// A formattable value that records how often it was actually formatted.
struct FormatProbe {
    int* calls;
};

} // namespace pt_test_log

template <>
struct std::formatter<pt_test_log::FormatProbe> {
    constexpr auto parse(std::format_parse_context& ctx) const { return ctx.begin(); }

    auto format(const pt_test_log::FormatProbe& probe, std::format_context& ctx) const {
        ++(*probe.calls);
        return std::format_to(ctx.out(), "probe");
    }
};

namespace {

using pt::LogLevel;

// The level is process-wide state (util/log.cpp), so a case must not leak its
// setting into the next one - inside a single test binary the cases share it.
class LogLevelGuard {
public:
    LogLevelGuard() noexcept : previous_(pt::log_level()) {}

    LogLevelGuard(const LogLevelGuard&) = delete;
    LogLevelGuard& operator=(const LogLevelGuard&) = delete;

    ~LogLevelGuard() { pt::set_log_level(previous_); }

private:
    LogLevel previous_;
};

} // namespace

TEST_CASE("the engine logs at info until told otherwise", "[util][log]") {
    // Every helper that touches the level restores it, so the default is still
    // observable here however the suites are ordered.
    REQUIRE(pt::log_level() == LogLevel::info);
}

TEST_CASE("should_log is a threshold and off blocks everything", "[util][log]") {
    const LogLevelGuard guard;

    pt::set_log_level(LogLevel::info);
    REQUIRE(pt::log_level() == LogLevel::info);
    REQUIRE(pt::should_log(LogLevel::info));
    REQUIRE(pt::should_log(LogLevel::warning));
    REQUIRE(pt::should_log(LogLevel::error));

    pt::set_log_level(LogLevel::warning);
    REQUIRE_FALSE(pt::should_log(LogLevel::info));
    REQUIRE(pt::should_log(LogLevel::warning));
    REQUIRE(pt::should_log(LogLevel::error));

    pt::set_log_level(LogLevel::error);
    REQUIRE_FALSE(pt::should_log(LogLevel::warning));
    REQUIRE(pt::should_log(LogLevel::error));

    // The comparison is '>=' against a level, so silence depends on 'off' being
    // the last enumerator. Reordering the enum breaks exactly this line.
    pt::set_log_level(LogLevel::off);
    REQUIRE_FALSE(pt::should_log(LogLevel::info));
    REQUIRE_FALSE(pt::should_log(LogLevel::warning));
    REQUIRE_FALSE(pt::should_log(LogLevel::error));
}

TEST_CASE("each level writes its own prefix and one newline", "[util][log]") {
    const LogLevelGuard guard;
    pt::set_log_level(LogLevel::info);

    const pt_test::ClogCapture capture;

    pt::log_info("plain");
    pt::log_warning("loud");
    pt::log_error("broken");

    REQUIRE(capture.text() == "plain\nwarning: loud\nerror: broken\n");
}

TEST_CASE("the wrappers filter, log_message does not", "[util][log]") {
    const LogLevelGuard guard;
    pt::set_log_level(LogLevel::off);

    const pt_test::ClogCapture capture;

    pt::log_info("dropped");
    pt::log_warning("dropped");
    pt::log_error("dropped");
    REQUIRE(capture.text().empty());

    // log_message is the sink, not a policy: it writes whatever it is handed.
    // Callers that want the level respected go through the wrappers, which is
    // what LogSilencer relies on across the suites.
    pt::log_message(LogLevel::error, "written anyway");
    REQUIRE(capture.text() == "error: written anyway\n");
}

TEST_CASE("a filtered record is never formatted", "[util][log]") {
    const LogLevelGuard guard;

    int calls = 0;
    const pt_test_log::FormatProbe probe{&calls};

    pt::set_log_level(LogLevel::off);
    {
        const pt_test::ClogCapture capture;
        pt::log_info("{}", probe);
        pt::log_warning("{}", probe);
        pt::log_error("{}", probe);
        REQUIRE(capture.text().empty());
    }

    // Formatting a per-tile statistic is not free. The wrappers check the level
    // before calling std::format so that a silenced build pays only the call.
    REQUIRE(calls == 0);

    pt::set_log_level(LogLevel::info);
    {
        const pt_test::ClogCapture capture;
        pt::log_info("{}", probe);
        REQUIRE(capture.text() == "probe\n");
    }
    REQUIRE(calls == 1);
}
