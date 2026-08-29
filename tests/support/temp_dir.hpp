#pragma once
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <string_view>
#include <system_error>

// Filesystem fixtures shared by the suites whose subject takes a path and opens
// it itself. Everything here is inline: this header is included by more than one
// translation unit.
namespace pt_test {

// One random token per process, computed once. catch_discover_tests gives every
// case its own process and `ctest -j` runs several at a time, so the name has to
// be unique across processes as well as within one.
[[nodiscard]] inline const std::string& process_token() {
    static const std::string token = [] {
        // Deliberately non-deterministic, unlike the engine's Sampler: two worker
        // processes must not agree on a directory name.
        std::random_device device;
        return std::to_string(device());
    }();
    return token;
}

/// A scratch directory that lives exactly as long as the object.
class TempDir {
public:
    explicit TempDir(std::string_view prefix = "pt_test") {
        // Cases inside one process run one after another, so a plain counter is
        // enough to separate the directories a single process creates.
        static int counter = 0;
        ++counter;

        path_ = std::filesystem::temp_directory_path() /
                (std::string(prefix) + "_" + process_token() + "_" + std::to_string(counter));

        std::error_code ec;
        std::filesystem::create_directories(path_, ec);
        REQUIRE(!ec);
    }

    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;

    // error_code overload, not the throwing one: this destructor can run while
    // unwinding from a failed expectation, where a second exception terminates.
    ~TempDir() {
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
    }

    /// Writes one file into the directory and returns its path. Binary mode: no
    /// newline translation, so the bytes on disk are the bytes in the literal.
    [[nodiscard]] std::filesystem::path write(std::string_view name, std::string_view contents) const {
        const std::filesystem::path file = path_ / name;

        std::ofstream stream(file, std::ofstream::binary);
        stream << contents;
        stream.close();
        REQUIRE(stream.good());

        return file;
    }

    [[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }

private:
    std::filesystem::path path_;
};

} // namespace pt_test
