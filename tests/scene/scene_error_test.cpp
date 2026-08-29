#include "pt/scene/scene_error.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <exception>
#include <stdexcept>
#include <string>

namespace {

using pt::SceneError;

} // namespace

TEST_CASE("an error starts as its message", "[scene][error]") {
    const SceneError error{"radius must be positive"};

    REQUIRE(std::string(error.what()) == "radius must be positive");

    // No location yet: the thrower knows what went wrong, not where in the file
    // it was. That context is added on the way out.
    REQUIRE(error.location().empty());
}

TEST_CASE("each frame prepends where it was", "[scene][error]") {
    SceneError error{"unknown type"};

    error.prepend("materials[2]");
    REQUIRE(error.location() == "materials[2]");
    REQUIRE(std::string(error.what()) == "materials[2]: unknown type");

    // Built during unwinding, innermost frame first, so each new segment goes in
    // front of what is already there. The result reads outside-in, the way the
    // file does - which is the point: "objects[3].material" is findable, and
    // "unknown type" on its own is not.
    error.prepend("objects[3].");
    REQUIRE(error.location() == "objects[3].materials[2]");
    REQUIRE(std::string(error.what()) == "objects[3].materials[2]: unknown type");

    error.prepend("scene.json > ");
    REQUIRE(std::string(error.what()) == "scene.json > objects[3].materials[2]: unknown type");
}

TEST_CASE("the composed message is what every catcher sees", "[scene][error]") {
    // main catches std::exception, and the loader's own frames catch SceneError.
    // what() is virtual all the way up, so the location survives however the
    // error is caught - a non-virtual override would silently hand the top-level
    // handler the bare message that runtime_error was constructed with.
    try {
        SceneError error{"expected a number"};
        error.prepend("camera.vfov");
        throw error;
    } catch (const std::exception& caught) {
        REQUIRE(std::string(caught.what()) == "camera.vfov: expected a number");
    }

    try {
        throw SceneError{"file not found"};
    } catch (const std::runtime_error& caught) {
        REQUIRE_THAT(std::string(caught.what()), Catch::Matchers::ContainsSubstring("file not found"));
    }
}

TEST_CASE("an error survives being copied", "[scene][error]") {
    SceneError original{"bad index"};
    original.prepend("objects[7]");

    // Exceptions are copied when thrown, and what() hands out a pointer into the
    // object's own storage. The copy owns its own strings, so the pointer stays
    // valid after the original goes away.
    const SceneError copy = original;
    REQUIRE(std::string(copy.what()) == "objects[7]: bad index");
    REQUIRE(copy.location() == "objects[7]");
}

TEST_CASE("an empty segment changes nothing", "[scene][error]") {
    SceneError error{"plain"};
    error.prepend("");

    // A frame with no name to add leaves the message alone rather than producing
    // a stray separator.
    REQUIRE(error.location().empty());
    REQUIRE(std::string(error.what()) == "plain");
}
