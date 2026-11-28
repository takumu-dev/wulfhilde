// The viewer's acceptance suite: the test maps in tests/content, loaded through the assets module
// as the viewer loads them.
#include "world_printing.hpp" // IWYU pragma: keep
#include <wulfhilde/assets/content_root.hpp>
#include <wulfhilde/assets/load_map.hpp>
#include <wulfhilde/assets/load_map_assets.hpp>
#include <wulfhilde/assets/material.hpp>
#include <wulfhilde/world/ambient_light.hpp>
#include <wulfhilde/world/ids.hpp>
#include <wulfhilde/world/map.hpp>
#include <wulfhilde/world/sector.hpp>
#include <wulfhilde/world/sky.hpp>

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <filesystem>
#include <format>
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

TEST_CASE("the sectors map is a concave hall, with a doorway into a gallery and a bay beyond, a "
          "stair onto a ledge, a low doorway into a nook, a step too high and a passage too low",
          "[viewer]") {
    const auto root = wulfhilde::ContentRoot::open(WULFHILDE_TEST_CONTENT);
    REQUIRE(root.has_value());

    const auto map = wulfhilde::load_map(*root, "sectors");

    REQUIRE(map.has_value());
    REQUIRE(map->sectors().size() == 10);
    const auto& hall = map->sector(map->start().location.sector);
    CHECK(hall.id == "hall");
    // Six corners, with the long sides of the arm along X split into walls that lie differently
    // and about the openings in them, the end of the arm along Y split about its doorway, and the
    // west side about the step too high.
    CHECK(hall.loops.front().size() == 18);
    CHECK(map->contains(map->start().location));
    CHECK(map->portals().size() == 9);
    constexpr std::array beyond{
        "gallery", "bay",  "stair-1",   "stair-2",     "ledge",
        "doorway", "nook", "high-step", "low-passage",
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
