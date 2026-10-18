#include "command_line.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <string_view>

TEST_CASE("the --help option asks for the usage", "[viewer]") {
    const std::array<std::string_view, 1> arguments{"--help"};

    const auto command_line = viewer::parse_command_line(arguments);

    REQUIRE(command_line.has_value());
    CHECK(command_line->help);
}

TEST_CASE("an unknown argument is a usage error that names it", "[viewer]") {
    const std::array<std::string_view, 2> arguments{"--help", "--fly"};

    const auto command_line = viewer::parse_command_line(arguments);

    REQUIRE_FALSE(command_line.has_value());
    CHECK(command_line.error().message == "unknown argument: --fly");
}
