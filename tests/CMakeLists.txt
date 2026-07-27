# Catch2 is a test-only dependency, so it is found here rather than at the
# root: nothing outside this directory links it. This placement is also
# load-bearing for the next step - Catch2Config.cmake appends its own
# directory to CMAKE_MODULE_PATH, and that change is scoped to this
# directory and its children, which is exactly where include(Catch) is
# needed.
#
# The "3" is not decoration. Catch2 v2 exports a target with the same name
# (Catch2::Catch2) and incompatible headers, so without the version
# constraint a stray v2 installation would satisfy this call and fail much
# later, at #include time, with an unhelpful error.
find_package(Catch2 3 REQUIRED)

add_executable(pathtracer_tests
  smoke_test.cpp
)

# PRIVATE throughout, and not as a default: an executable has no consumers,
# so PUBLIC and INTERFACE would have nothing to propagate to.
#
# Catch2::Catch2WithMain alone - not the pair that vcpkg's heuristic message
# suggested. Upstream declares target_link_libraries(Catch2WithMain PUBLIC
# Catch2), so the framework arrives transitively. Linking the bare
# Catch2::Catch2 target is what you do in the other case: when you write
# your own main() and drive the run through Catch::Session yourself.
#
# pt::warnings is the one that is easy to forget and expensive to forget.
# The project applies its warning policy per target rather than globally, so
# a target that omits it silently leaves the policy - including -Werror in
# Debug. Test code is code.
target_link_libraries(pathtracer_tests PRIVATE
  Catch2::Catch2WithMain
  pt::core
  pt::warnings
)

# Catch.cmake ships with Catch2 and provides catch_discover_tests. It is
# reachable via include() only because find_package(Catch2) above appended
# Catch2's own config directory to CMAKE_MODULE_PATH - which is the reason
# find_package lives in this file rather than at the root.
include(Catch)

# One CTest entry per Catch2 test case, rather than one entry for the whole
# binary (which is what add_test would give, and what pbrt-v4 chose).
#
# The mechanism: a POST_BUILD command runs the freshly built binary, asks it
# to enumerate its test cases, and generates add_test() calls from the answer.
# Two consequences worth remembering - the CTest list is only accurate after
# a build, and under the asan-ubsan preset that enumeration run is itself
# instrumented, so a sanitizer finding there fails the build, not the tests.
#
# The finer granularity is what buys process isolation: a test case that
# segfaults or hangs takes down its own process and reports as a single
# failure, instead of destroying every result in the binary. That matters
# more here than in most projects - BVH and Monte Carlo tests are exactly
# the kind that hang or trap.
#
# ADD_TAGS_AS_LABELS turns Catch2 tags into CTest labels, so "[smoke]"
# becomes usable as `ctest -L smoke`. It carries an existing piece of
# information into a second system rather than inventing a new one.
#
# Deliberately not set: WORKING_DIRECTORY (no test reads from disk yet),
# TEST_PREFIX (only one test binary exists), DISCOVERY_MODE (POST_BUILD is
# correct when not cross-compiling).
catch_discover_tests(pathtracer_tests
  ADD_TAGS_AS_LABELS
)
