#pragma once
#include <iostream>
#include <sstream>
#include <streambuf>
#include <string>

namespace pt_test {

/// Redirects std::clog into a string for the lifetime of the object and puts the
/// original buffer back on exit. The logging layer has no injectable sink, so
/// capturing the stream is the only way to assert on what a component reported.
class ClogCapture {
public:
    ClogCapture() : previous_(std::clog.rdbuf(buffer_.rdbuf())) {}

    ClogCapture(const ClogCapture&) = delete;
    ClogCapture& operator=(const ClogCapture&) = delete;

    ~ClogCapture() { std::clog.rdbuf(previous_); }

    /// Everything written since construction. Records accumulate; the caller
    /// asserts on the whole transcript rather than on one line at a time.
    [[nodiscard]] std::string text() const { return buffer_.str(); }

private:
    // Declaration order is load-bearing: previous_ is initialised from
    // buffer_.rdbuf(), and members are constructed in declaration order.
    std::ostringstream buffer_;
    std::streambuf* previous_;
};

} // namespace pt_test
