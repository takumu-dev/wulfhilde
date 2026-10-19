#include <wulfhilde/world/map_load_error.hpp>

#include <wulfhilde/world/map_problem.hpp>

#include <catch2/catch_test_macros.hpp>

#include <format>

using wulfhilde::MapLoadError;
using wulfhilde::MapProblem;
using wulfhilde::MapProblemCode;

TEST_CASE("a problem formats as its message, then its pointer in parentheses", "[world]") {
    const MapProblem problem{
        .code = MapProblemCode::unknown_sector,
        .message = R"(no sector has the id "yard")",
        .pointer = "/start/sector",
    };
    CHECK(std::format("{}", problem) == R"(no sector has the id "yard" (/start/sector))");
}

TEST_CASE("a map's load error formats as its problems, each on a line of its own", "[world]") {
    const MapLoadError error{
        .problems =
            {
                {
                    .code = MapProblemCode::too_few_walls,
                    .message = "the loop has 2 walls, but a loop needs at least 3",
                    .pointer = "/sectors/0/loops/0",
                },
                {
                    .code = MapProblemCode::unknown_sector,
                    .message = R"(no sector has the id "yard")",
                    .pointer = "/start/sector",
                },
            },
    };
    CHECK(std::format("{}", error) ==
          "the loop has 2 walls, but a loop needs at least 3 (/sectors/0/loops/0)\n"
          R"(no sector has the id "yard" (/start/sector))");
    CHECK(std::format("{}", MapLoadError{.problems = {error.problems.at(1)}}) ==
          R"(no sector has the id "yard" (/start/sector))");
}
