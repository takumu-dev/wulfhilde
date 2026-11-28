#include <wulfhilde/assets/load_map.hpp>

#include "test_files.hpp"
#include "world_printing.hpp" // IWYU pragma: keep
#include <wulfhilde/assets/content_root.hpp>
#include <wulfhilde/core/angle.hpp>
#include <wulfhilde/core/vector2.hpp>
#include <wulfhilde/world/ambient_light.hpp>
#include <wulfhilde/world/exposure.hpp>
#include <wulfhilde/world/ids.hpp>
#include <wulfhilde/world/map.hpp>
#include <wulfhilde/world/map_problem.hpp>
#include <wulfhilde/world/portal.hpp>
#include <wulfhilde/world/portal_link.hpp>
#include <wulfhilde/world/sector.hpp>
#include <wulfhilde/world/sky.hpp>
#include <wulfhilde/world/surface.hpp>
#include <wulfhilde/world/surface_ref.hpp>

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

using wulfhilde::AmbientLight;
using wulfhilde::Angle;
using wulfhilde::BackdropId;
using wulfhilde::ContentRoot;
using wulfhilde::Cover;
using wulfhilde::Exposure;
using wulfhilde::load_map;
using wulfhilde::Map;
using wulfhilde::MapProblemCode;
using wulfhilde::MaterialId;
using wulfhilde::PortalId;
using wulfhilde::PortalLink;
using wulfhilde::SectorId;
using wulfhilde::Sky;
using wulfhilde::Surface;
using wulfhilde::WallId;

namespace {

Map load_valid(const ContentRoot& root, std::string_view path) {
    auto map = load_map(root, path);
    if (!map) {
        FAIL(std::format("{}", map.error()));
    }
    return std::move(*map);
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
    CHECK(hall.floor.cover == Cover{Surface{
                                  .material = MaterialId{0},
                                  .offset = {.x = 0.25F, .y = 0.5F},
                                  .scale = {.x = 2.0F, .y = 3.0F},
                                  .rotation = Angle::degrees(90.0F),
                                  .nits = 12.5F,
                              }});
    CHECK(hall.ceiling.height == 3.0F);
    CHECK(hall.ceiling.cover == Cover{Surface{.material = MaterialId{1}}});
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
    CHECK(wall.id == "hall-east");
    CHECK(wall.upper == Surface{
                            .material = MaterialId{1},
                            .offset = {.x = 0.0F, .y = 0.5F},
                            .nits = 30.0F,
                        });
    CHECK(wall.lower == wall.surface);
    CHECK(wall.link == PortalLink{.portal = PortalId{0}, .wall = WallId{6}});
    CHECK(map.wall(WallId{0}).surface == Surface{.material = MaterialId{2}});
    CHECK(map.wall(WallId{0}).properties.empty());
    CHECK(map.wall(WallId{0}).id == std::nullopt);

    const auto& closet_west = map.wall(WallId{6});
    CHECK(closet_west.id == "closet-west");
    CHECK(closet_west.upper == Surface{.material = MaterialId{2}});
    CHECK(closet_west.lower ==
          Surface{.material = MaterialId{2}, .rotation = Angle::degrees(90.0F)});
    CHECK(closet_west.link == PortalLink{.portal = PortalId{0}, .wall = WallId{1}});

    REQUIRE(map.portals().size() == 1);
    CHECK(map.portal(PortalId{0}).a == wulfhilde::SurfaceRef{WallId{1}});
    CHECK(map.portal(PortalId{0}).b == wulfhilde::SurfaceRef{WallId{6}});
    CHECK(map.portal(PortalId{0}).properties == R"({ "sound": "creak" })");

    const auto backdrops = map.backdrops();
    CHECK(std::vector<std::string>(backdrops.begin(), backdrops.end()) ==
          std::vector<std::string>{"sky/abyss", "sky/dusk"});
    const auto& closet = map.sector(SectorId{1});
    CHECK(closet.id == "closet");
    CHECK(closet.floor.height == 0.0F);
    CHECK(closet.floor.cover == Cover{Sky{.backdrop = BackdropId{0}}});
    CHECK(closet.ceiling.height == 2.5F);
    CHECK(closet.ceiling.cover == Cover{Sky{.backdrop = BackdropId{1}, .nits = 50.0F}});
    CHECK(closet.ambient ==
          AmbientLight{.color = {.r = 0.5F, .g = 0.75F, .b = 1.0F}, .lux = 20.0F});
    CHECK(closet.properties.empty());

    CHECK(map.start().location.sector == SectorId{1});
    CHECK(map.start().location.point == wulfhilde::Vector3{.x = 4.5F, .y = 2.0F, .z = 0.0F});
    CHECK(map.start().yaw == Angle::degrees(180.0F));
    CHECK(map.start().pitch == Angle::degrees(-10.0F));
    CHECK(map.exposure() == Exposure{.min = -2.5F, .max = 12.0F, .compensation = 0.5F});
    CHECK(map.properties() == "{}");
}

TEST_CASE("a map's file may leave out every optional member", "[assets]") {
    const auto map = load_valid(open_root(test_files() / "valid"), "hall");

    CHECK(map.start().pitch == Angle{});
    CHECK(map.sector(SectorId{0}).ambient == AmbientLight{});
    CHECK(map.exposure() == Exposure{});
    CHECK(map.sector(SectorId{0}).properties.empty());
    CHECK(map.properties().empty());
    CHECK(map.portals().empty());
    CHECK(map.wall(WallId{0}).id == std::nullopt);
    CHECK(map.wall(WallId{0}).upper == map.wall(WallId{0}).surface);
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

TEST_CASE("loading a map times its parse and the making of the map, apart", "[assets]") {
    const auto root = open_root(test_files() / "valid");
    wulfhilde::MapLoadTimes times;

    const auto map = load_map(root, "everything", times);

    REQUIRE(map.has_value());
    CHECK(times.parse > std::chrono::nanoseconds{});
    CHECK(times.create > std::chrono::nanoseconds{});
}

TEST_CASE("a map whose file doesn't parse is timed only as far as its parse", "[assets]") {
    const auto root = open_root(test_files() / "invalid");
    wulfhilde::MapLoadTimes times;

    const auto map = load_map(root, "syntax_error", times);

    REQUIRE(!map.has_value());
    CHECK(times.parse > std::chrono::nanoseconds{});
    CHECK(times.create == std::chrono::nanoseconds{});
}
