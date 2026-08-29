#include "pt/util/overloaded.hpp"
#include <catch2/catch_test_macros.hpp>
#include <string>
#include <type_traits>
#include <variant>

TEST_CASE("Overloaded dispatches on the alternative the variant holds", "[util][overloaded]") {
    using Value = std::variant<int, double, std::string>;

    const auto describe = pt::Overloaded{
        [](int) { return std::string("int"); },
        [](double) { return std::string("double"); },
        [](const std::string&) { return std::string("string"); }};

    REQUIRE(std::visit(describe, Value{1}) == "int");
    REQUIRE(std::visit(describe, Value{1.0}) == "double");
    REQUIRE(std::visit(describe, Value{std::string("x")}) == "string");
}

TEST_CASE("the aggregate deduces its own lambda types", "[util][overloaded]") {
    const auto increment = pt::Overloaded{[](int v) { return v + 1; }};

    // No deduction guide is written: C++20 deduces class template arguments for
    // aggregates. Giving Overloaded a constructor would end that silently, and
    // every `pt::Overloaded{...}` call site would stop compiling at once.
    STATIC_REQUIRE(std::is_aggregate_v<decltype(increment)>);
    REQUIRE(increment(1) == 2);
}
