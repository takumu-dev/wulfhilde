// The viewer's acceptance suite: the test maps in tests/content, loaded through the assets module
// as the viewer loads them.
#include "world_printing.hpp" // IWYU pragma: keep
#include <wulfhilde/assets/content_root.hpp>
#include <wulfhilde/assets/load_map.hpp>
#include <wulfhilde/assets/load_map_assets.hpp>
#include <wulfhilde/assets/material.hpp>
#include <wulfhilde/core/angle.hpp>
#include <wulfhilde/core/rigid_transform.hpp>
#include <wulfhilde/core/rotation.hpp>
#include <wulfhilde/core/vector2.hpp>
#include <wulfhilde/core/vector3.hpp>
#include <wulfhilde/world/ambient_light.hpp>
#include <wulfhilde/world/ids.hpp>
#include <wulfhilde/world/map.hpp>
#include <wulfhilde/world/portal.hpp>
#include <wulfhilde/world/portal_link.hpp>
#include <wulfhilde/world/sector.hpp>
#include <wulfhilde/world/sky.hpp>
#include <wulfhilde/world/wall.hpp>

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <format>
#include <numbers>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace {

// The sector of a map with an id, which the map must have.
const wulfhilde::Sector& sector_named(const wulfhilde::Map& map, std::string_view id) {
    const auto found = map.find_sector(id);
    REQUIRE(found.has_value());
    return map.sector(found.value_or(wulfhilde::SectorId{0}));
}

// The sky a floor or a ceiling shows, or nothing for a surface.
std::optional<wulfhilde::Sky> sky_of(const wulfhilde::FloorOrCeiling& floor_or_ceiling) {
    const auto* const sky = std::get_if<wulfhilde::Sky>(&floor_or_ceiling.cover);
    return sky != nullptr ? std::optional(*sky) : std::nullopt;
}

// Each pair of a sector and another it shares space with, by their ids, in order of the first's
// ID and then the second's.
std::vector<std::pair<std::string, std::string>> overlaps_named(const wulfhilde::Map& map) {
    std::vector<std::pair<std::string, std::string>> named;
    for (const auto& sector : map.sectors()) {
        const auto id = map.find_sector(sector.id).value_or(wulfhilde::SectorId{0});
        for (const auto other : map.overlapping(id)) {
            named.emplace_back(sector.id, map.sector(other).id);
        }
    }
    return named;
}

// The slopes map, whose assets must load too.
wulfhilde::Map load_slopes() {
    const auto root = wulfhilde::ContentRoot::open(WULFHILDE_TEST_CONTENT);
    REQUIRE(root.has_value());
    auto map = wulfhilde::load_map(*root, "slopes");
    REQUIRE(map.has_value());
    REQUIRE(wulfhilde::load_map_assets(*root, *map).has_value());
    return *std::move(map);
}

// The slope of a floor or a ceiling, which must slope.
wulfhilde::Slope slope_of(const wulfhilde::FloorOrCeiling& floor_or_ceiling) {
    REQUIRE(floor_or_ceiling.slope.has_value());
    return floor_or_ceiling.slope.value_or(wulfhilde::Slope{.hinge = wulfhilde::WallId{0}});
}

// The gradient of a floor or a ceiling, which must slope.
float gradient_of(const wulfhilde::FloorOrCeiling& floor_or_ceiling) {
    return slope_of(floor_or_ceiling).gradient;
}

} // namespace

TEST_CASE("every test map loads without a problem, and so do its assets", "[viewer]") {
    const std::filesystem::path content = WULFHILDE_TEST_CONTENT;
    const auto root = wulfhilde::ContentRoot::open(content);
    REQUIRE(root.has_value());

    constexpr std::string_view extension = wulfhilde::map_extension;
    std::size_t maps = 0;
    for (const auto& entry : std::filesystem::directory_iterator(content)) {
        const auto name = entry.path().filename().string();
        if (!name.ends_with(extension)) {
            continue;
        }
        ++maps;
        CAPTURE(name);
        const auto map = wulfhilde::load_map(*root, name.substr(0, name.size() - extension.size()));
        if (!map) {
            FAIL_CHECK(std::format("{}", map.error()));
            continue;
        }
        const auto assets = wulfhilde::load_map_assets(*root, *map);
        if (!assets) {
            FAIL_CHECK(std::format("{}", assets.error()));
        }
    }
    CHECK(maps > 0);
}

TEST_CASE("the sectors map is a concave hall around a pillar and a dais, with a doorway into a "
          "gallery and a bay beyond, a stair onto a ledge, a low doorway into a nook, a step too "
          "high and a passage too low",
          "[viewer]") {
    const auto root = wulfhilde::ContentRoot::open(WULFHILDE_TEST_CONTENT);
    REQUIRE(root.has_value());

    const auto map = wulfhilde::load_map(*root, "sectors");

    REQUIRE(map.has_value());
    REQUIRE(map->sectors().size() == 11);
    const auto& hall = map->sector(map->start().location.sector);
    CHECK(hall.id == "hall");
    // Six corners, with the long sides of the arm along X split into walls that lie differently
    // and about the openings in them, the end of the arm along Y split about its doorway, and the
    // west side about the step too high.
    CHECK(hall.loops.front().size() == 18);
    // A pillar's inner loop, and the dais's, whose four walls are sides of portals into it.
    REQUIRE(hall.loops.size() == 3);
    CHECK(hall.loops.at(1).size() == 4);
    CHECK(std::ranges::none_of(hall.loops.at(1), [&map](wulfhilde::WallId wall) {
        return map->wall(wall).link.has_value();
    }));
    CHECK(std::ranges::all_of(hall.loops.at(2), [&map](wulfhilde::WallId wall) {
        return map->wall(wall).link.has_value();
    }));
    CHECK(map->contains(map->start().location));
    CHECK(map->portals().size() == 13);
    constexpr std::array beyond{
        "gallery", "bay",  "stair-1",   "stair-2",     "ledge",
        "doorway", "nook", "high-step", "low-passage", "dais",
    };
    CHECK(std::ranges::all_of(beyond,
                              [&map](const char* id) { return map->find_sector(id).has_value(); }));

    const auto assets = wulfhilde::load_map_assets(*root, *map);
    REQUIRE(assets.has_value());
    CHECK(assets->materials.size() == 5);
    // The floor's, the ceiling's, the walls' and the strips' debug grids, each read once, as a
    // colour.
    CHECK(assets->images.size() == 5);
}

TEST_CASE("the outdoors map has two neighbouring skies of different backdrops, one at nits of its "
          "own, and a bridge over an abyss whose floor is a sky",
          "[viewer]") {
    const auto root = wulfhilde::ContentRoot::open(WULFHILDE_TEST_CONTENT);
    REQUIRE(root.has_value());

    const auto map = wulfhilde::load_map(*root, "outdoors");

    REQUIRE(map.has_value());
    CHECK(map->sector(map->start().location.sector).id == "yard");
    const auto backdrops = map->backdrops();
    CHECK(std::vector<std::string>(backdrops.begin(), backdrops.end()) ==
          std::vector<std::string>{"backdrops/compass_blue", "backdrops/compass_amber"});
    const wulfhilde::Sky blue{.backdrop = wulfhilde::BackdropId{0}};
    CHECK(sky_of(sector_named(*map, "yard").ceiling) == blue);
    CHECK(sky_of(sector_named(*map, "field").ceiling) ==
          wulfhilde::Sky{.backdrop = wulfhilde::BackdropId{1}, .nits = 500.0F});
    CHECK(sky_of(sector_named(*map, "bridge").floor) == std::nullopt);
    CHECK(sky_of(sector_named(*map, "abyss-west").floor) == blue);
    CHECK(sky_of(sector_named(*map, "abyss-east").floor) == blue);

    const auto assets = wulfhilde::load_map_assets(*root, *map);
    REQUIRE(assets.has_value());
    // Both backdrops shine at 2,000 nits of their own, which the field's sky dims.
    REQUIRE(assets->backdrops.size() == 2);
    CHECK(assets->backdrops.at(0).nits == 2'000.0F);
    CHECK(assets->backdrops.at(1).nits == 2'000.0F);
}

namespace {

// The nits that each wall of a sector carrying a material sets, in the order of its loop.
std::vector<std::optional<float>> nits_of_walls_carrying(const wulfhilde::Map& map,
                                                         const wulfhilde::Sector& sector,
                                                         std::string_view material) {
    std::vector<std::optional<float>> nits;
    for (const auto wall : sector.loops.front()) {
        const auto& surface = map.wall(wall).surface;
        if (map.materials().subspan(surface.material.index(), 1).front() == material) {
            nits.push_back(surface.nits);
        }
    }
    return nits;
}

} // namespace

TEST_CASE("the lights map has one lamp material at its own nits and at a surface's, and an annex "
          "under ambient light of its own beyond a doorway",
          "[viewer]") {
    const auto root = wulfhilde::ContentRoot::open(WULFHILDE_TEST_CONTENT);
    REQUIRE(root.has_value());

    const auto map = wulfhilde::load_map(*root, "lights");

    REQUIRE(map.has_value());
    const auto& hall = map->sector(map->start().location.sector);
    CHECK(hall.id == "hall");
    CHECK(map->portals().size() == 1);
    CHECK(nits_of_walls_carrying(*map, hall, "materials/lamp") ==
          std::vector<std::optional<float>>{std::nullopt, 80.0F});
    CHECK(hall.ambient ==
          wulfhilde::AmbientLight{.color = {.r = 1.0F, .g = 1.0F, .b = 1.0F}, .lux = 300.0F});
    CHECK(sector_named(*map, "annex").ambient ==
          wulfhilde::AmbientLight{.color = {.r = 1.0F, .g = 0.85F, .b = 0.7F}, .lux = 75.0F});
}

TEST_CASE("the lights map's lamp material glows at 400 nits of its own", "[viewer]") {
    const auto root = wulfhilde::ContentRoot::open(WULFHILDE_TEST_CONTENT);
    REQUIRE(root.has_value());
    const auto map = wulfhilde::load_map(*root, "lights");
    REQUIRE(map.has_value());
    const auto materials = map->materials();
    const auto lamp = static_cast<std::size_t>(std::ranges::find(materials, "materials/lamp") -
                                               materials.begin());
    REQUIRE(lamp < materials.size());

    const auto assets = wulfhilde::load_map_assets(*root, *map);

    REQUIRE(assets.has_value());
    const auto emission = assets->materials.at(lamp).emission.value_or(wulfhilde::Emission{});
    CHECK(emission.nits == 400.0F);
}

TEST_CASE(
    "the overlap map has a hut whose hall shares the yard's space, and a bridge over a passage "
    "that shares none",
    "[viewer]") {
    const auto root = wulfhilde::ContentRoot::open(WULFHILDE_TEST_CONTENT);
    REQUIRE(root.has_value());

    const auto map = wulfhilde::load_map(*root, "overlap");

    REQUIRE(map.has_value());
    CHECK(map->sectors().size() == 7);
    CHECK(map->portals().size() == 11);
    CHECK(map->sector(map->start().location.sector).id == "yard");
    CHECK(overlaps_named(*map) ==
          std::vector<std::pair<std::string, std::string>>{{"yard", "hall"}, {"hall", "yard"}});
    // The hall is lit as a room is, a tenth as bright as the yard outside.
    CHECK(sector_named(*map, "hall").ambient.lux == 1'000.0F);
    CHECK(sector_named(*map, "yard").ambient.lux == 10'000.0F);
}

TEST_CASE(
    "the slopes map's ramp rises from the hall's floor to the landing's, hinged on a diagonal "
    "wall",
    "[viewer]") {
    const auto map = load_slopes();
    const auto& ramp = sector_named(map, "ramp");
    const auto& hinge = map.wall(slope_of(ramp.floor).hinge);
    CHECK(hinge.start.x != hinge.end.x);
    CHECK(hinge.start.y != hinge.end.y);
    CHECK(hinge.link.has_value());
    CHECK(!ramp.ceiling.slope.has_value());

    const auto ramp_id = map.find_sector("ramp").value_or(wulfhilde::SectorId{0});
    const auto landing_id = map.find_sector("landing").value_or(wulfhilde::SectorId{0});
    CHECK(std::abs(map.floor_height({.sector = ramp_id, .point = {.x = 11.0F, .y = 7.0F}})) <
          1e-5F);
    CHECK(std::abs(map.floor_height({.sector = ramp_id, .point = {.x = 13.0F, .y = 9.0F}}) -
                   map.floor_height({.sector = landing_id})) < 1e-5F);
}

TEST_CASE(
    "the slopes map has a sloped ceiling, and a floor and a ceiling that slope different ways",
    "[viewer]") {
    const auto map = load_slopes();

    const auto& attic = sector_named(map, "attic");
    CHECK(!attic.floor.slope.has_value());
    CHECK(gradient_of(attic.ceiling) < 0.0F);

    // The opposed sector's floor and ceiling are hinged on walls at right angles.
    const auto& opposed = sector_named(map, "opposed");
    const auto& floor_hinge = map.wall(slope_of(opposed.floor).hinge);
    const auto& ceiling_hinge = map.wall(slope_of(opposed.ceiling).hinge);
    CHECK(wulfhilde::dot(floor_hinge.end - floor_hinge.start,
                         ceiling_hinge.end - ceiling_hinge.start) == 0.0F);
    CHECK(gradient_of(opposed.floor) > 0.0F);
    CHECK(gradient_of(opposed.ceiling) < 0.0F);
}

TEST_CASE("the slopes map has slopes of 30 and 60 degrees side by side", "[viewer]") {
    const auto map = load_slopes();
    CHECK(std::abs(std::atan(gradient_of(sector_named(map, "gentle").floor)) -
                   (std::numbers::pi_v<float> / 6.0F)) < 1e-6F);
    CHECK(std::abs(std::atan(gradient_of(sector_named(map, "steep").floor)) -
                   (std::numbers::pi_v<float> / 3.0F)) < 1e-6F);
}

namespace {

// The transform through the portal one of whose sides is a wall with an id, from that wall's
// sector into the sector beyond, and the id of the sector beyond.
std::pair<wulfhilde::RigidTransform, std::string> through_wall(const wulfhilde::Map& map,
                                                               std::string_view id) {
    for (const auto& portal : map.portals()) {
        for (const auto& side : {portal.a, portal.b}) {
            const auto* const wall_id = std::get_if<wulfhilde::WallId>(&side);
            if (wall_id == nullptr || map.wall(*wall_id).id != id) {
                continue;
            }
            const auto& link = map.wall(*wall_id).link;
            REQUIRE(link.has_value());
            if (link) {
                return {link->transform, map.sector(map.wall(link->wall).sector).id};
            }
        }
    }
    FAIL(std::format("no portal has the wall {} as a side", id));
    return {};
}

// The portals map, whose assets must load too.
wulfhilde::Map load_portals() {
    const auto root = wulfhilde::ContentRoot::open(WULFHILDE_TEST_CONTENT);
    REQUIRE(root.has_value());
    auto map = wulfhilde::load_map(*root, "portals");
    REQUIRE(map.has_value());
    REQUIRE(wulfhilde::load_map_assets(*root, *map).has_value());
    return *std::move(map);
}

} // namespace

TEST_CASE("the portals map closes a loop of three corners with a portal turned 90 degrees, and "
          "reaches a distant annex through a translated portal",
          "[viewer]") {
    const auto map = load_portals();

    const auto [turn, beyond_turn] = through_wall(map, "east-leg-end");
    CHECK(beyond_turn == "start");
    CHECK(wulfhilde::approx_equal(
        turn.rotation,
        wulfhilde::Rotation::from_yaw_pitch_roll(wulfhilde::Angle::degrees(-90.0F), {}, {}),
        wulfhilde::Angle::degrees(1e-3F)));

    const auto [translation, beyond_translation] = through_wall(map, "start-west");
    CHECK(beyond_translation == "annex");
    CHECK(translation.rotation == wulfhilde::Rotation::identity());
    CHECK(translation.translation == wulfhilde::Vector3{.x = 2987.0F, .y = 3000.0F, .z = 100.0F});
}

TEST_CASE("the portals map has a corridor whose facing ends lead into each other", "[viewer]") {
    const auto map = load_portals();
    CHECK(through_wall(map, "mirror-east").second == "mirror");
    CHECK(through_wall(map, "mirror-west").second == "mirror");
}

TEST_CASE("the portals map has a hub whose four portals all lead back into it", "[viewer]") {
    const auto map = load_portals();
    for (const auto* const side : {"hub-north", "hub-east", "hub-south", "hub-west"}) {
        CAPTURE(side);
        const auto [transform, beyond] = through_wall(map, side);
        CHECK(beyond == "hub");
        CHECK(transform.rotation == wulfhilde::Rotation::identity());
        CHECK(wulfhilde::length(transform.translation) == 8.0F);
    }
}
