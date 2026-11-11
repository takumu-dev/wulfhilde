#include <wulfhilde/world/map_load_error.hpp>

#include <wulfhilde/world/map_problem.hpp>
#include <wulfhilde/world/text_position.hpp>

#include <catch2/catch_test_macros.hpp>

#include <format>

using wulfhilde::MapLoadError;
using wulfhilde::MapProblem;
using wulfhilde::MapProblemCode;
using wulfhilde::TextPosition;

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

TEST_CASE("a problem with a position in its file formats with the line and column first",
          "[world]") {
    const MapProblem problem{
        .code = MapProblemCode::unknown_sector,
        .message = R"(no sector has the id "yard")",
        .pointer = "/start/sector",
        .position = TextPosition{.line = 12, .column = 7},
    };
    CHECK(std::format("{}", problem) == R"(12:7: no sector has the id "yard" (/start/sector))");
}

TEST_CASE("a problem in a whole file formats without a pointer", "[world]") {
    const MapProblem problem{
        .code = MapProblemCode::syntax_error,
        .message = "unexpected end of the text",
        .position = TextPosition{.line = 3, .column = 1},
    };
    CHECK(std::format("{}", problem) == "3:1: unexpected end of the text");
    CHECK(std::format("{}", MapProblem{.code = MapProblemCode::missing_file,
                                       .message = "no file is at the asset path"}) ==
          "no file is at the asset path");
}

TEST_CASE("a load error of a file formats each problem after the file's name, as compilers do",
          "[world]") {
    const MapLoadError error{
        .file = "maps/e1m1.map.json",
        .problems =
            {
                {
                    .code = MapProblemCode::unknown_member,
                    .message = R"(the floor has no member "hieght")",
                    .pointer = "/sectors/0/floor/hieght",
                    .position = TextPosition{.line = 12, .column = 7},
                },
                {
                    .code = MapProblemCode::missing_file,
                    .message = "no file is at the asset path",
                },
            },
    };
    CHECK(std::format("{}", error) ==
          "maps/e1m1.map.json:12:7: the floor has no member \"hieght\" (/sectors/0/floor/hieght)\n"
          "maps/e1m1.map.json: no file is at the asset path");
}
