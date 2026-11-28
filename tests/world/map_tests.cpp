#include <wulfhilde/world/map.hpp>

#include "descriptions.hpp"
#include "world_printing.hpp" // IWYU pragma: keep
#include <wulfhilde/core/angle.hpp>
#include <wulfhilde/core/vector2.hpp>
#include <wulfhilde/world/ambient_light.hpp>
#include <wulfhilde/world/exposure.hpp>
#include <wulfhilde/world/ids.hpp>
#include <wulfhilde/world/location.hpp>
#include <wulfhilde/world/map_description.hpp>
#include <wulfhilde/world/opening.hpp>
#include <wulfhilde/world/portal.hpp>
#include <wulfhilde/world/portal_link.hpp>
#include <wulfhilde/world/sector.hpp>
#include <wulfhilde/world/sky.hpp>
#include <wulfhilde/world/surface.hpp>
#include <wulfhilde/world/surface_ref.hpp>

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

using wulfhilde::AmbientLight;
using wulfhilde::Angle;
using wulfhilde::BackdropId;
using wulfhilde::Cover;
using wulfhilde::Exposure;
using wulfhilde::Location;
using wulfhilde::MaterialId;
using wulfhilde::PortalId;
using wulfhilde::PortalLink;
using wulfhilde::SectorId;
using wulfhilde::Sky;
using wulfhilde::SkyDescription;
using wulfhilde::Surface;
using wulfhilde::SurfaceRef;
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
    CHECK(hall.floor.cover == Cover{Surface{
                                  .material = MaterialId{0},
                                  .offset = {.x = 0.25F, .y = 0.5F},
                                  .scale = {.x = 2.0F, .y = 0.5F},
                                  .rotation = Angle::degrees(30.0F),
                              }});
    CHECK(hall.ceiling.height == 2.5F);
    CHECK(hall.ceiling.cover == Cover{Surface{.material = MaterialId{1}}});
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
    CHECK(map.sector(SectorId{1}).floor.cover == Cover{Surface{.material = MaterialId{1}}});
    CHECK(map.sector(SectorId{1}).ceiling.cover == Cover{Surface{.material = MaterialId{3}}});
}

TEST_CASE("a floor or a ceiling may be a sky, which shows a backdrop at its own brightness or the "
          "backdrop's",
          "[world]") {
    auto description = hall_map();
    description.sectors.at(0).floor.sky = SkyDescription{.backdrop = "test/abyss"};
    description.sectors.at(0).ceiling.sky = SkyDescription{.backdrop = "test/day", .nits = 250.0F};
    const auto map = make_map(description);

    const auto& hall = map.sector(SectorId{0});
    CHECK(hall.floor.height == 0.0F);
    CHECK(hall.floor.cover == Cover{Sky{.backdrop = BackdropId{0}}});
    CHECK(hall.ceiling.height == 3.0F);
    CHECK(hall.ceiling.cover == Cover{Sky{.backdrop = BackdropId{1}, .nits = 250.0F}});
    // A sky carries no material, so the floor's and the ceiling's materials go unnamed.
    const auto materials = map.materials();
    CHECK(std::vector<std::string>(materials.begin(), materials.end()) ==
          std::vector<std::string>{"test/wall"});
}

TEST_CASE("each distinct backdrop gets one ID, in the order the description first names it",
          "[world]") {
    auto description = hall_map();
    description.sectors.push_back(closet_sector());
    description.sectors.at(0).ceiling.sky = SkyDescription{.backdrop = "test/day"};
    description.sectors.at(1).floor.sky = SkyDescription{.backdrop = "test/abyss"};
    description.sectors.at(1).ceiling.sky = SkyDescription{.backdrop = "test/day", .nits = 5.0F};
    const auto map = make_map(description);

    const auto backdrops = map.backdrops();
    CHECK(std::vector<std::string>(backdrops.begin(), backdrops.end()) ==
          std::vector<std::string>{"test/day", "test/abyss"});
    CHECK(map.sector(SectorId{1}).floor.cover == Cover{Sky{.backdrop = BackdropId{1}}});
    CHECK(map.sector(SectorId{1}).ceiling.cover ==
          Cover{Sky{.backdrop = BackdropId{0}, .nits = 5.0F}});
}

TEST_CASE("a map without skies has no backdrops", "[world]") {
    CHECK(make_map(hall_map()).backdrops().empty());
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

TEST_CASE("a sector's inner loops follow its outer loop, their walls numbered on from its",
          "[world]") {
    auto description = pillared_hall_map();
    description.sectors.push_back(closet_sector());
    const auto map = make_map(description);

    using Loops = std::vector<std::vector<WallId>>;
    CHECK(map.sector(SectorId{0}).loops == Loops{{WallId{0}, WallId{1}, WallId{2}, WallId{3}},
                                                 {WallId{4}, WallId{5}, WallId{6}, WallId{7}}});
    CHECK(map.sector(SectorId{1}).loops == Loops{{WallId{8}, WallId{9}, WallId{10}}});

    const auto& pillar_west = map.wall(WallId{4});
    CHECK(pillar_west.sector == SectorId{0});
    CHECK(pillar_west.start == Vector2{.x = 1.0F, .y = 1.0F});
    CHECK(pillar_west.end == Vector2{.x = 1.0F, .y = 2.0F});
    // The inner loop's last wall runs back to its own first corner, not the outer loop's.
    CHECK(map.wall(WallId{7}).start == Vector2{.x = 2.0F, .y = 1.0F});
    CHECK(map.wall(WallId{7}).end == Vector2{.x = 1.0F, .y = 1.0F});
}

TEST_CASE("an inner loop's walls may be sides of portals into the sector inside it", "[world]") {
    const auto map = make_map(dais_map());

    REQUIRE(map.portals().size() == 4);
    CHECK(map.wall(WallId{4}).link == PortalLink{.portal = PortalId{0}, .wall = WallId{11}});
    CHECK(map.wall(WallId{11}).link == PortalLink{.portal = PortalId{0}, .wall = WallId{4}});
    CHECK(map.wall(WallId{7}).link == PortalLink{.portal = PortalId{3}, .wall = WallId{8}});
    CHECK(map.opening(WallId{5}) == wulfhilde::Opening{.bottom = 0.5F, .top = 3.0F});
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

TEST_CASE("a map's exposure is its own, or else from 1 to 16 EV100 with no compensation",
          "[world]") {
    CHECK(make_map(hall_map()).exposure() ==
          Exposure{.min = 1.0F, .max = 16.0F, .compensation = 0.0F});

    auto description = hall_map();
    description.exposure = {.min = -2.0F, .max = 9.5F, .compensation = 1.5F};
    CHECK(make_map(description).exposure() ==
          Exposure{.min = -2.0F, .max = 9.5F, .compensation = 1.5F});
}

TEST_CASE("the map, its sectors and its walls keep their properties, which the engine ignores",
          "[world]") {
    auto description = hall_map();
    description.properties = R"({"music": "calm", "par": [3, 45]})";
    description.sectors.at(0).properties = R"({"secret": true})";
    description.sectors.at(0).loops.at(0).at(2).properties = R"({"switch": "lights"})";
    const auto map = make_map(description);
    CHECK(map.properties() == R"({"music": "calm", "par": [3, 45]})");
    CHECK(map.sector(SectorId{0}).properties == R"({"secret": true})");
    CHECK(map.wall(WallId{2}).properties == R"({"switch": "lights"})");
    CHECK(map.wall(WallId{1}).properties.empty());

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

TEST_CASE("a portal has the walls it links as its sides, and keeps its properties", "[world]") {
    auto description = doorway_map();
    description.portals.at(0).properties = R"({"locked": false})";
    const auto map = make_map(description);

    REQUIRE(map.portals().size() == 1);
    const auto& portal = map.portal(PortalId{0});
    CHECK(portal.a == SurfaceRef{WallId{3}});
    CHECK(portal.b == SurfaceRef{WallId{7}});
    CHECK(portal.properties == R"({"locked": false})");
}

TEST_CASE("each side of a portal links through it to the other, and a solid wall links nowhere",
          "[world]") {
    const auto map = make_map(doorway_map());

    CHECK(map.wall(WallId{3}).link == PortalLink{.portal = PortalId{0}, .wall = WallId{7}});
    CHECK(map.wall(WallId{7}).link == PortalLink{.portal = PortalId{0}, .wall = WallId{3}});
    CHECK(map.wall(WallId{7}).sector == SectorId{1});
    for (const auto solid : {WallId{0}, WallId{2}, WallId{4}, WallId{6}, WallId{8}}) {
        CAPTURE(solid);
        CHECK(map.wall(solid).link == std::nullopt);
    }
}

TEST_CASE("a map without portals has none", "[world]") {
    CHECK(make_map(hall_map()).portals().empty());
}

TEST_CASE("a wall keeps its id, if it has one", "[world]") {
    const auto map = make_map(doorway_map());
    CHECK(map.wall(WallId{3}).id == "hall-door");
    CHECK(map.wall(WallId{7}).id == "gallery-door");
    CHECK(map.wall(WallId{0}).id == std::nullopt);
}

TEST_CASE("a wall's strips have surfaces of their own, or else the wall's", "[world]") {
    auto description = doorway_map();
    auto& door = description.sectors.at(0).loops.at(0).at(3);
    door.surface = {.material = "test/door", .scale = {.x = 2.0F, .y = 2.0F}};
    door.upper = {.material = "test/lintel", .offset = {.y = 0.5F}};
    description.sectors.at(1).loops.at(0).at(1).lower = {
        .material = "test/sill",
        .rotation = Angle::degrees(90.0F),
    };
    const auto map = make_map(description);

    const auto material = [&map](std::string_view path) {
        const auto materials = map.materials();
        return MaterialId{
            static_cast<std::uint32_t>(std::ranges::find(materials, path) - materials.begin())};
    };
    const auto& hall_door = map.wall(WallId{3});
    CHECK(hall_door.surface ==
          Surface{.material = material("test/door"), .scale = {.x = 2.0F, .y = 2.0F}});
    CHECK(hall_door.upper == Surface{.material = material("test/lintel"), .offset = {.y = 0.5F}});
    CHECK(hall_door.lower == hall_door.surface);

    const auto& gallery_door = map.wall(WallId{7});
    CHECK(gallery_door.upper == Surface{.material = material("test/wall")});
    CHECK(gallery_door.lower ==
          Surface{.material = material("test/sill"), .rotation = Angle::degrees(90.0F)});
}
