#include "command_line.hpp"

#include "math_printing.hpp" // IWYU pragma: keep
#include <wulfhilde/core/angle.hpp>
#include <wulfhilde/core/vector3.hpp>

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <filesystem>
#include <format>
#include <span>
#include <string_view>
#include <utility>

namespace {

using wulfhilde::Angle;

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

TEST_CASE("the --camera option names a sector, a point and a facing, with no roll unless one is "
          "given",
          "[viewer]") {
    const std::array<std::string_view, 3> without_roll{
        "e1m1.map.json",
        "--camera",
        "hall,11,1,1.65,165,-5",
    };
    const auto parsed = parse_valid(without_roll).camera;
    REQUIRE(parsed.has_value());
    const auto camera = parsed.value_or(viewer::CameraOption{});
    CHECK(camera.sector == "hall");
    CHECK(camera.point == wulfhilde::Vector3{.x = 11.0F, .y = 1.0F, .z = 1.65F});
    CHECK(camera.yaw == Angle::degrees(165.0F));
    CHECK(camera.pitch == Angle::degrees(-5.0F));
    CHECK(camera.roll == Angle::degrees(0.0F));

    const std::array<std::string_view, 3> with_roll{
        "--camera",
        "shaft,-0.5,2e1,3,-180,90,-30.25",
        "e1m1.map.json",
    };
    const auto parsed_rolled = parse_valid(with_roll).camera;
    REQUIRE(parsed_rolled.has_value());
    const auto rolled = parsed_rolled.value_or(viewer::CameraOption{});
    CHECK(rolled.sector == "shaft");
    CHECK(rolled.point == wulfhilde::Vector3{.x = -0.5F, .y = 20.0F, .z = 3.0F});
    CHECK(rolled.yaw == Angle::degrees(-180.0F));
    CHECK(rolled.pitch == Angle::degrees(90.0F));
    CHECK(rolled.roll == Angle::degrees(-30.25F));
}

TEST_CASE("without --camera, the command line names no camera", "[viewer]") {
    const std::array<std::string_view, 1> arguments{"e1m1.map.json"};

    CHECK(!parse_valid(arguments).camera.has_value());
}

TEST_CASE("a --camera of other than six or seven values is a usage error", "[viewer]") {
    for (const std::string_view camera : {"hall,1,2", "hall,1,2,3,4,5,6,7", ""}) {
        CAPTURE(camera);
        const std::array<std::string_view, 3> arguments{"e1m1.map.json", "--camera", camera};

        const auto error = parse_wrong(arguments);

        CHECK(error.code == viewer::UsageErrorCode::invalid_value);
        CHECK(std::format("{}", error) ==
              std::format("--camera needs <sector>,<x>,<y>,<z>,<yaw>,<pitch>[,<roll>], not {}",
                          camera));
    }
}

TEST_CASE("a --camera value that isn't a number is a usage error that names it", "[viewer]") {
    const std::array<std::pair<std::string_view, std::string_view>, 6> cases{
        {
            {"hall,one,2,3,4,5", "--camera's x isn't a number: one"},
            {"hall,1,2m,3,4,5", "--camera's y isn't a number: 2m"},
            {"hall,1,2,,4,5", "--camera's z isn't a number: "},
            {"hall,1,2,3,inf,5", "--camera's yaw isn't a number: inf"},
            {"hall,1,2,3,4,nan", "--camera's pitch isn't a number: nan"},
            {"hall,1,2,3,4,5, 6", "--camera's roll isn't a number:  6"},
        },
    };
    for (const auto& [camera, message] : cases) {
        CAPTURE(camera);
        const std::array<std::string_view, 3> arguments{"e1m1.map.json", "--camera", camera};

        const auto error = parse_wrong(arguments);

        CHECK(error.code == viewer::UsageErrorCode::invalid_value);
        CHECK(std::format("{}", error) == message);
    }
}

TEST_CASE("a --camera with no sector is a usage error", "[viewer]") {
    const std::array<std::string_view, 3> arguments{"e1m1.map.json", "--camera", ",1,2,3,4,5"};

    const auto error = parse_wrong(arguments);

    CHECK(error.code == viewer::UsageErrorCode::invalid_value);
    CHECK(std::format("{}", error) == "--camera names no sector: ,1,2,3,4,5");
}

TEST_CASE("a --camera facing out of range is a usage error", "[viewer]") {
    const std::array<std::pair<std::string_view, std::string_view>, 4> cases{
        {
            {"hall,1,2,3,180.5,0", "--camera's yaw must be from -180 to 180 degrees, not 180.5"},
            {"hall,1,2,3,0,-90.5", "--camera's pitch must be from -90 to 90 degrees, not -90.5"},
            {"hall,1,2,3,0,91", "--camera's pitch must be from -90 to 90 degrees, not 91"},
            {"hall,1,2,3,0,0,-200", "--camera's roll must be from -180 to 180 degrees, not -200"},
        },
    };
    for (const auto& [camera, message] : cases) {
        CAPTURE(camera);
        const std::array<std::string_view, 3> arguments{"e1m1.map.json", "--camera", camera};

        const auto error = parse_wrong(arguments);

        CHECK(error.code == viewer::UsageErrorCode::out_of_range);
        CHECK(std::format("{}", error) == message);
    }
}

TEST_CASE("the --fullscreen option asks for a window that covers the display", "[viewer]") {
    const std::array<std::string_view, 1> windowed{"e1m1.map.json"};
    CHECK(!parse_valid(windowed).fullscreen);

    const std::array<std::string_view, 2> fullscreen{"--fullscreen", "e1m1.map.json"};
    CHECK(parse_valid(fullscreen).fullscreen);
}

TEST_CASE("the --samples option takes a sample count that Vulkan can name, 4 by default",
          "[viewer]") {
    const std::array<std::string_view, 1> by_default{"e1m1.map.json"};
    CHECK(parse_valid(by_default).samples == 4);

    for (const std::size_t samples : {1UZ, 2UZ, 4UZ, 8UZ, 16UZ, 32UZ, 64UZ}) {
        CAPTURE(samples);
        const auto value = std::format("{}", samples);
        const std::array<std::string_view, 3> arguments{"e1m1.map.json", "--samples", value};

        CHECK(parse_valid(arguments).samples == samples);
    }
}

TEST_CASE("a --samples count that Vulkan can't name is a usage error", "[viewer]") {
    for (const std::string_view samples : {"0", "3", "128", "6"}) {
        CAPTURE(samples);
        const std::array<std::string_view, 3> arguments{"e1m1.map.json", "--samples", samples};

        const auto error = parse_wrong(arguments);

        CHECK(error.code == viewer::UsageErrorCode::out_of_range);
        CHECK(std::format("{}", error) ==
              std::format("--samples must be 1, 2, 4, 8, 16, 32 or 64, not {}", samples));
    }
}

TEST_CASE("a --samples count that isn't a whole number is a usage error", "[viewer]") {
    for (const std::string_view samples : {"four", "4x", "-4", "4.0", ""}) {
        CAPTURE(samples);
        const std::array<std::string_view, 3> arguments{"e1m1.map.json", "--samples", samples};

        const auto error = parse_wrong(arguments);

        CHECK(error.code == viewer::UsageErrorCode::invalid_value);
        CHECK(std::format("{}", error) ==
              std::format("--samples needs a whole number, not {}", samples));
    }
}

TEST_CASE("the --camera and --samples options need a value after them", "[viewer]") {
    const std::array<std::pair<std::string_view, std::string_view>, 2> cases{
        {
            {"--camera", "--camera needs <sector>,<x>,<y>,<z>,<yaw>,<pitch>[,<roll>] after it"},
            {"--samples", "--samples needs a sample count after it"},
        },
    };
    for (const auto& [option, message] : cases) {
        CAPTURE(option);
        const std::array<std::string_view, 2> arguments{"e1m1.map.json", option};

        const auto error = parse_wrong(arguments);

        CHECK(error.code == viewer::UsageErrorCode::missing_value);
        CHECK(std::format("{}", error) == message);
    }
}

TEST_CASE("the --camera and --samples options each name one value", "[viewer]") {
    const std::array<std::string_view, 5> cameras{
        "e1m1.map.json", "--camera", "hall,1,2,3,4,5", "--camera", "yard,1,2,3,4,5",
    };
    const auto camera = parse_wrong(cameras);
    CHECK(camera.code == viewer::UsageErrorCode::extra_argument);
    CHECK(std::format("{}", camera) == "a second camera: yard,1,2,3,4,5, after hall,1,2,3,4,5");

    const std::array<std::string_view, 5> samples{
        "e1m1.map.json", "--samples", "8", "--samples", "4",
    };
    const auto count = parse_wrong(samples);
    CHECK(count.code == viewer::UsageErrorCode::extra_argument);
    CHECK(std::format("{}", count) == "a second sample count: 4, after 8");
}
