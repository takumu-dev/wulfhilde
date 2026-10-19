#include "command_line.hpp"

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <filesystem>
#include <format>
#include <span>
#include <string_view>

namespace {

viewer::CommandLine parse_valid(std::span<const std::string_view> arguments) {
    auto command_line = viewer::parse_command_line(arguments);
    if (!command_line) {
        FAIL(std::format("{}", command_line.error()));
    }
    return *command_line;
}

viewer::UsageError parse_wrong(std::span<const std::string_view> arguments) {
    const auto command_line = viewer::parse_command_line(arguments);
    REQUIRE(!command_line.has_value());
    return command_line.error();
}

} // namespace

TEST_CASE("the --help option asks for the usage", "[viewer]") {
    const std::array<std::string_view, 1> arguments{"--help"};

    CHECK(parse_valid(arguments).help);
}

TEST_CASE("an unknown argument is a usage error that names it", "[viewer]") {
    const std::array<std::string_view, 2> arguments{"--help", "--fly"};

    const auto error = parse_wrong(arguments);

    CHECK(error.code == viewer::UsageErrorCode::unknown_argument);
    CHECK(std::format("{}", error) == "unknown argument: --fly");
}

TEST_CASE("a map file's content root is its own directory, unless --content names another",
          "[viewer]") {
    const std::array<std::string_view, 1> alone{"content/maps/e1m1.map.json"};
    const auto own = parse_valid(alone);
    CHECK(own.map == std::filesystem::path("content/maps/e1m1.map.json"));
    CHECK(own.content == std::filesystem::path("content/maps"));
    CHECK(own.map_asset_path == "e1m1");

    const std::array<std::string_view, 3> named{
        "content/maps/e1m1.map.json",
        "--content",
        "content",
    };
    const auto other = parse_valid(named);
    CHECK(other.content == std::filesystem::path("content"));
    CHECK(other.map_asset_path == "maps/e1m1");

    const std::array<std::string_view, 3> first{
        "--content",
        "content/",
        "content/maps/e1m1.map.json",
    };
    CHECK(parse_valid(first).map_asset_path == "maps/e1m1");
}

TEST_CASE("a map file in the working directory has the working directory as its content root",
          "[viewer]") {
    const std::array<std::string_view, 1> arguments{"e1m1.map.json"};

    const auto command_line = parse_valid(arguments);

    CHECK(command_line.content == std::filesystem::path("."));
    CHECK(command_line.map_asset_path == "e1m1");
}

TEST_CASE("a map file is found in its content root however the two paths are written", "[viewer]") {
    const auto content = std::filesystem::current_path() / "content";
    const auto absolute_map = (content / "maps" / "e1m1.map.json").string();
    const auto absolute_content = content.string();
    const std::array<std::string_view, 3> absolute_map_only{absolute_map, "--content", "content"};
    CHECK(parse_valid(absolute_map_only).map_asset_path == "maps/e1m1");

    const std::array<std::string_view, 3> absolute_content_only{
        "content/./maps/../maps/e1m1.map.json",
        "--content",
        absolute_content,
    };
    CHECK(parse_valid(absolute_content_only).map_asset_path == "maps/e1m1");
}

TEST_CASE("a map file outside its content root is a usage error", "[viewer]") {
    for (const std::string_view content : {
             "content/maps/e1m1",
             "other",
             "content/maps/e1m1.map.json",
         }) {
        CAPTURE(content);
        const std::array<std::string_view, 3> arguments{
            "content/maps/e1m1.map.json",
            "--content",
            content,
        };

        const auto error = parse_wrong(arguments);

        CHECK(error.code == viewer::UsageErrorCode::map_outside_content);
        CHECK(std::format("{}", error) ==
              std::format("content/maps/e1m1.map.json isn't in the content root, {}", content));
    }
}

TEST_CASE("a map file must have the extension of a map", "[viewer]") {
    for (const std::string_view map : {"content/e1m1.json", "content/.map.json", "content/"}) {
        CAPTURE(map);
        const std::array<std::string_view, 1> arguments{map};

        const auto error = parse_wrong(arguments);

        CHECK(error.code == viewer::UsageErrorCode::not_a_map_file);
        CHECK(std::format("{}", error) ==
              std::format("{} isn't a map file, whose name ends in .map.json", map));
    }
}

TEST_CASE("the viewer opens exactly one map file", "[viewer]") {
    const std::array<std::string_view, 2> none{"--content", "content"};
    const auto missing = parse_wrong(none);
    CHECK(missing.code == viewer::UsageErrorCode::missing_map);
    CHECK(std::format("{}", missing) == "no map file to open");

    const std::array<std::string_view, 2> two{"e1m1.map.json", "e1m2.map.json"};
    const auto extra = parse_wrong(two);
    CHECK(extra.code == viewer::UsageErrorCode::extra_argument);
    CHECK(std::format("{}", extra) == "a second map file: e1m2.map.json, after e1m1.map.json");
}

TEST_CASE("the --content option needs a directory after it", "[viewer]") {
    const std::array<std::string_view, 2> arguments{"e1m1.map.json", "--content"};

    const auto error = parse_wrong(arguments);

    CHECK(error.code == viewer::UsageErrorCode::missing_value);
    CHECK(std::format("{}", error) == "--content needs a directory after it");
}

TEST_CASE("the --content option names one content root", "[viewer]") {
    const std::array<std::string_view, 5> arguments{
        "e1m1.map.json", "--content", ".", "--content", "content",
    };

    const auto error = parse_wrong(arguments);

    CHECK(error.code == viewer::UsageErrorCode::extra_argument);
    CHECK(std::format("{}", error) == "a second content root: content, after .");
}
