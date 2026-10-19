#include <wulfhilde/world/map.hpp>

#include "descriptions.hpp"
#include "world_printing.hpp" // IWYU pragma: keep
#include <wulfhilde/core/angle.hpp>
#include <wulfhilde/core/vector2.hpp>
#include <wulfhilde/world/ambient_light.hpp>
#include <wulfhilde/world/ids.hpp>
#include <wulfhilde/world/location.hpp>
#include <wulfhilde/world/surface.hpp>

#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <string>
#include <vector>

using wulfhilde::AmbientLight;
using wulfhilde::Angle;
using wulfhilde::Location;
using wulfhilde::MaterialId;
using wulfhilde::SectorId;
using wulfhilde::Surface;
using wulfhilde::Vector2;
using wulfhilde::WallId;

TEST_CASE("a map made from a valid description finds each sector by its id", "[world]") {
    auto description = hall_map();
    description.sectors.push_back(closet_sector());
    const auto map = make_map(description);

    REQUIRE(map.sectors().size() == 2);
    CHECK(map.find_sector("hall") == SectorId{0});
    CHECK(map.find_sector("closet") == SectorId{1});
    CHECK(map.find_sector("yard") == std::nullopt);
    CHECK(map.sector(SectorId{0}).id == "hall");
    CHECK(map.sector(SectorId{1}).id == "closet");
}

TEST_CASE("a sector keeps its floor and its ceiling, each with its height and surface", "[world]") {
    auto description = hall_map();
    description.sectors.at(0).floor = {
        .height = -0.5F,
        .material = "test/floor",
        .offset = {.x = 0.25F, .y = 0.5F},
        .scale = {.x = 2.0F, .y = 0.5F},
        .rotation = Angle::degrees(30.0F),
    };
    description.sectors.at(0).ceiling.height = 2.5F;
    const auto map = make_map(description);

    const auto& hall = map.sector(SectorId{0});
    CHECK(hall.floor.height == -0.5F);
    CHECK(hall.floor.surface == Surface{
                                    .material = MaterialId{0},
                                    .offset = {.x = 0.25F, .y = 0.5F},
                                    .scale = {.x = 2.0F, .y = 0.5F},
                                    .rotation = Angle::degrees(30.0F),
                                });
    CHECK(hall.ceiling.height == 2.5F);
    CHECK(hall.ceiling.surface == Surface{.material = MaterialId{1}});
}

TEST_CASE("each distinct material gets one ID, in the order the description first names it",
          "[world]") {
    auto description = hall_map();
    description.sectors.push_back(closet_sector());
    description.sectors.at(1).floor.material = "test/ceiling";
    description.sectors.at(1).ceiling.material = "test/stone";
    const auto map = make_map(description);

    const auto materials = map.materials();
    CHECK(std::vector<std::string>(materials.begin(), materials.end()) ==
          std::vector<std::string>{"test/floor", "test/ceiling", "test/wall", "test/stone"});
    CHECK(map.sector(SectorId{1}).floor.surface.material == MaterialId{1});
    CHECK(map.sector(SectorId{1}).ceiling.surface.material == MaterialId{3});
}

TEST_CASE("each wall runs to the start of the next one in its loop, and knows its sector",
          "[world]") {
    auto description = hall_map();
    description.sectors.push_back(closet_sector());
    description.sectors.at(1).loops.at(0).at(1).surface = {
        .material = "test/brick",
        .offset = {.x = 0.5F},
        .scale = {.x = 1.0F, .y = 2.0F},
        .rotation = Angle::degrees(-90.0F),
    };
    const auto map = make_map(description);

    using Loops = std::vector<std::vector<WallId>>;
    CHECK(map.sector(SectorId{0}).loops == Loops{{WallId{0}, WallId{1}, WallId{2}, WallId{3}}});
    CHECK(map.sector(SectorId{1}).loops == Loops{{WallId{4}, WallId{5}, WallId{6}}});

    const auto& hall_west = map.wall(WallId{3});
    CHECK(hall_west.sector == SectorId{0});
    CHECK(hall_west.start == Vector2{.x = 0.0F, .y = 4.0F});
    CHECK(hall_west.end == Vector2{.x = 0.0F, .y = 0.0F});
    CHECK(hall_west.surface == Surface{.material = MaterialId{2}});

    const auto& closet_east = map.wall(WallId{5});
    CHECK(closet_east.sector == SectorId{1});
    CHECK(closet_east.start == Vector2{.x = 12.0F, .y = 0.0F});
    CHECK(closet_east.end == Vector2{.x = 12.0F, .y = 1.0F});
    CHECK(closet_east.surface == Surface{
                                     .material = MaterialId{3},
                                     .offset = {.x = 0.5F},
                                     .scale = {.x = 1.0F, .y = 2.0F},
                                     .rotation = Angle::degrees(-90.0F),
                                 });
    CHECK(map.wall(WallId{6}).end == Vector2{.x = 10.0F, .y = 0.0F});
}

TEST_CASE("a sector's ambient light is its own, or else the map's, or else black", "[world]") {
    constexpr AmbientLight dusk{.color = {.r = 0.5F, .g = 0.5F, .b = 1.0F}, .lux = 3.0F};
    constexpr AmbientLight candlelight{.color = {.r = 1.0F, .g = 0.75F, .b = 0.5F}, .lux = 40.0F};
    auto description = hall_map();
    description.sectors.push_back(closet_sector());
    description.sectors.at(1).ambient = candlelight;
    CHECK(make_map(description).sector(SectorId{0}).ambient == AmbientLight{});
    CHECK(make_map(description).sector(SectorId{1}).ambient == candlelight);

    description.ambient = dusk;
    CHECK(make_map(description).sector(SectorId{0}).ambient == dusk);
    CHECK(make_map(description).sector(SectorId{1}).ambient == candlelight);
}

TEST_CASE("the map and its sectors keep their properties, which the engine ignores", "[world]") {
    auto description = hall_map();
    description.properties = R"({"music": "calm", "par": [3, 45]})";
    description.sectors.at(0).properties = R"({"secret": true})";
    const auto map = make_map(description);
    CHECK(map.properties() == R"({"music": "calm", "par": [3, 45]})");
    CHECK(map.sector(SectorId{0}).properties == R"({"secret": true})");

    CHECK(make_map(hall_map()).properties().empty());
}

TEST_CASE("the start position is a location in the sector it names, with a facing", "[world]") {
    auto description = hall_map();
    description.sectors.push_back(closet_sector());
    description.start = {
        .sector = "closet",
        .point = {.x = 11.5F, .y = 0.25F, .z = 0.0F},
        .yaw = Angle::degrees(135.0F),
        .pitch = Angle::degrees(-10.0F),
    };
    const auto map = make_map(description);
    CHECK(map.start().location ==
          Location{.sector = SectorId{1}, .point = {.x = 11.5F, .y = 0.25F, .z = 0.0F}});
    CHECK(map.start().yaw == Angle::degrees(135.0F));
    CHECK(map.start().pitch == Angle::degrees(-10.0F));
}
