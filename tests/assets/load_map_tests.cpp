#include <wulfhilde/assets/load_map.hpp>

#include "test_files.hpp"
#include "world_printing.hpp" // IWYU pragma: keep
#include <wulfhilde/assets/content_root.hpp>
#include <wulfhilde/core/angle.hpp>
#include <wulfhilde/core/vector2.hpp>
#include <wulfhilde/world/ambient_light.hpp>
#include <wulfhilde/world/ids.hpp>
#include <wulfhilde/world/map.hpp>
#include <wulfhilde/world/map_problem.hpp>
#include <wulfhilde/world/surface.hpp>

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <filesystem>
#include <format>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

using wulfhilde::AmbientLight;
using wulfhilde::Angle;
using wulfhilde::ContentRoot;
using wulfhilde::load_map;
using wulfhilde::Map;
using wulfhilde::MapProblemCode;
using wulfhilde::MaterialId;
using wulfhilde::SectorId;
using wulfhilde::Surface;
using wulfhilde::WallId;

namespace {

ContentRoot open_root(const std::filesystem::path& directory) {
    auto root = ContentRoot::open(directory);
    REQUIRE(root.has_value());
    return std::move(*root);
}

Map load_valid(const ContentRoot& root, std::string_view path) {
    auto map = load_map(root, path);
    if (!map) {
        FAIL(std::format("{}", map.error()));
    }
    return std::move(*map);
}

// The asset path of each map under a directory, and of each file beside them, without its
// extension, as long as the file's name has the extension.
std::vector<std::string> asset_paths(const std::filesystem::path& directory,
                                     std::string_view extension) {
    std::vector<std::string> paths;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(directory)) {
        const auto name = entry.path().lexically_relative(directory).generic_string();
        if (entry.is_regular_file() && name.ends_with(extension)) {
            paths.push_back(name.substr(0, name.size() - extension.size()));
        }
    }
    std::ranges::sort(paths);
    return paths;
}

} // namespace

TEST_CASE("each invalid map reports exactly the problems written beside it", "[assets]") {
    const auto invalid = test_files() / "invalid";
    const auto root = open_root(invalid);
    const auto maps = asset_paths(invalid, wulfhilde::map_extension);
    CHECK(maps == asset_paths(invalid, ".expected"));
    CHECK(maps.size() >= 20);

    for (const auto& path : maps) {
        CAPTURE(path);
        const auto map = load_map(root, path);
        REQUIRE(!map.has_value());
        CHECK(std::format("{}\n", map.error()) == read_text(invalid / (path + ".expected")));
    }
}

TEST_CASE("a map's file loads as the map it describes, with defaults for what it leaves out",
          "[assets]") {
    const auto map = load_valid(open_root(test_files() / "valid"), "everything");

    const auto materials = map.materials();
    CHECK(std::vector<std::string>(materials.begin(), materials.end()) ==
          std::vector<std::string>{"stone/floor", "plaster", "brick", "brick/dark"});

    REQUIRE(map.sectors().size() == 2);
    const auto& hall = map.sector(SectorId{0});
    CHECK(hall.id == "hall");
    CHECK(hall.floor.height == -0.5F);
    CHECK(hall.floor.surface == Surface{
                                    .material = MaterialId{0},
                                    .offset = {.x = 0.25F, .y = 0.5F},
                                    .scale = {.x = 2.0F, .y = 3.0F},
                                    .rotation = Angle::degrees(90.0F),
                                });
    CHECK(hall.ceiling.height == 3.0F);
    CHECK(hall.ceiling.surface == Surface{.material = MaterialId{1}});
    CHECK(hall.ambient == AmbientLight{.color = {.r = 1.0F, .g = 0.5F, .b = 0.25F}, .lux = 5.0F});
    CHECK(hall.loops == std::vector<std::vector<WallId>>{
                            {WallId{0}, WallId{1}, WallId{2}, WallId{3}},
                        });
    CHECK(hall.properties == R"({
        "secret": true,
        "notes": ["a } and a \" in a string", { "nested": [1, 2.5e1, null] }]
      })");

    const auto& wall = map.wall(WallId{1});
    CHECK(wall.sector == SectorId{0});
    CHECK(wall.start == wulfhilde::Vector2{.x = 4.0F, .y = 0.0F});
    CHECK(wall.end == wulfhilde::Vector2{.x = 4.0F, .y = 4.0F});
    CHECK(wall.surface == Surface{
                              .material = MaterialId{3},
                              .offset = {.x = 1.0F, .y = 2.0F},
                              .scale = {.x = 0.5F, .y = 4.0F},
                              .rotation = Angle::degrees(-45.0F),
                          });
    CHECK(wall.properties == R"({ "switch": "lights" })");
    CHECK(map.wall(WallId{0}).surface == Surface{.material = MaterialId{2}});
    CHECK(map.wall(WallId{0}).properties.empty());

    const auto& closet = map.sector(SectorId{1});
    CHECK(closet.id == "closet");
    CHECK(closet.ambient ==
          AmbientLight{.color = {.r = 0.5F, .g = 0.75F, .b = 1.0F}, .lux = 20.0F});
    CHECK(closet.properties.empty());

    CHECK(map.start().location.sector == SectorId{1});
    CHECK(map.start().location.point == wulfhilde::Vector3{.x = 11.0F, .y = 0.25F, .z = 0.0F});
    CHECK(map.start().yaw == Angle::degrees(180.0F));
    CHECK(map.start().pitch == Angle::degrees(-10.0F));
    CHECK(map.properties() == "{}");
}

TEST_CASE("a map's file may leave out every optional member", "[assets]") {
    const auto map = load_valid(open_root(test_files() / "valid"), "hall");

    CHECK(map.start().pitch == Angle{});
    CHECK(map.sector(SectorId{0}).ambient == AmbientLight{});
    CHECK(map.sector(SectorId{0}).properties.empty());
    CHECK(map.properties().empty());
}

TEST_CASE("a map's file may begin with a byte-order mark", "[assets]") {
    const auto directory = scratch_directory("load_map_byte_order_mark");
    auto text = read_text(test_files() / "valid" / "hall.map.json");
    text.replace(text.rfind('}'), 1, R"(, "properties": { "byte order": "marked" } })");
    write_text(directory / "marked.map.json", "\xEF\xBB\xBF" + text);

    const auto map = load_valid(open_root(directory), "marked");

    CHECK(map.properties() == R"({ "byte order": "marked" })");
}

TEST_CASE("a map that isn't there is a problem of its whole file", "[assets]") {
    const auto map = load_map(open_root(test_files() / "valid"), "maps/nothing");

    REQUIRE(!map.has_value());
    CHECK(map.error().file == "maps/nothing.map.json");
    REQUIRE(map.error().problems.size() == 1);
    CHECK(map.error().problems.at(0).code == MapProblemCode::missing_file);
    CHECK(std::format("{}", map.error()) == "maps/nothing.map.json: there is no such file");
}

TEST_CASE("a map's file that is a directory can't be read", "[assets]") {
    const auto directory = scratch_directory("load_map_directory");
    std::filesystem::create_directory(directory / "hall.map.json");

    const auto map = load_map(open_root(directory), "hall");

    REQUIRE(!map.has_value());
    REQUIRE(map.error().problems.size() == 1);
    CHECK(map.error().problems.at(0).code == MapProblemCode::unreadable_file);
    CHECK(std::format("{}", map.error()) ==
          "hall.map.json: the file can't be read, since it isn't a regular file");
}

TEST_CASE("a map's asset path must be an asset path, so that it stays in its content root",
          "[assets]") {
    for (const std::string_view path : {"Valid/Everything", "../valid/everything", ""}) {
        CAPTURE(path);
        const auto map = load_map(open_root(test_files() / "invalid"), path);

        REQUIRE(!map.has_value());
        REQUIRE(map.error().problems.size() == 1);
        CHECK(map.error().problems.at(0).code == MapProblemCode::invalid_asset_path);
        CHECK(std::format("{}", map.error()) ==
              std::format(R"({}.map.json: "{}" isn't an asset path, which is segments of a-z, )"
                          "0-9, _ and - separated by / and without an extension",
                          path, path));
    }
}

TEST_CASE("a map's file is found through a symbolic link in its content root", "[assets]") {
    const auto directory = scratch_directory("load_map_link");
    std::error_code error;
    std::filesystem::create_symlink(test_files() / "valid" / "hall.map.json",
                                    directory / "linked.map.json", error);
    if (error) {
        SKIP("this system can't make a symbolic link: " << error.message());
    }

    const auto map = load_valid(open_root(directory), "linked");

    CHECK(map.find_sector("hall") == SectorId{0});
}
