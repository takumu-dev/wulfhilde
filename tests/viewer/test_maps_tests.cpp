// The viewer's acceptance suite: the test maps in tests/content, loaded through the assets module
// as the viewer loads them.
#include "world_printing.hpp" // IWYU pragma: keep
#include <wulfhilde/assets/content_root.hpp>
#include <wulfhilde/assets/load_map.hpp>
#include <wulfhilde/assets/load_map_assets.hpp>
#include <wulfhilde/world/ambient_light.hpp>
#include <wulfhilde/world/ids.hpp>
#include <wulfhilde/world/map.hpp>
#include <wulfhilde/world/sector.hpp>
#include <wulfhilde/world/sky.hpp>

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <format>
#include <optional>
#include <string>
#include <string_view>
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

TEST_CASE("the sectors map is a concave hall, with a doorway into a gallery and a bay beyond",
          "[viewer]") {
    const auto root = wulfhilde::ContentRoot::open(WULFHILDE_TEST_CONTENT);
    REQUIRE(root.has_value());

    const auto map = wulfhilde::load_map(*root, "sectors");

    REQUIRE(map.has_value());
    REQUIRE(map->sectors().size() == 3);
    const auto& hall = map->sector(map->start().location.sector);
    CHECK(hall.id == "hall");
    // Six corners, with the long sides of the arm along X split into walls that lie differently,
    // and the end of the arm along Y split about its doorway.
    CHECK(hall.loops.front().size() == 11);
    CHECK(map->contains(map->start().location));
    CHECK(map->portals().size() == 2);
    CHECK(map->find_sector("gallery").has_value());
    CHECK(map->find_sector("bay").has_value());

    const auto assets = wulfhilde::load_map_assets(*root, *map);
    REQUIRE(assets.has_value());
    CHECK(assets->materials.size() == 4);
    // The floor's, the ceiling's, the walls' and the upper strip's debug grids, each read once, as
    // a colour.
    CHECK(assets->images.size() == 4);
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
    const auto lamp = std::ranges::find(map->materials(), "materials/lamp");
    REQUIRE(lamp != map->materials().end());
    std::vector<std::optional<float>> lamps;
    for (const auto wall : hall.loops.front()) {
        const auto& surface = map->wall(wall).surface;
        if (map->materials()[surface.material.index()] == "materials/lamp") {
            lamps.push_back(surface.nits);
        }
    }
    CHECK(lamps == std::vector<std::optional<float>>{std::nullopt, 80.0F});
    const wulfhilde::AmbientLight white{.color = {.r = 1.0F, .g = 1.0F, .b = 1.0F}, .lux = 300.0F};
    CHECK(hall.ambient == white);
    CHECK(sector_named(*map, "annex").ambient ==
          wulfhilde::AmbientLight{.color = {.r = 1.0F, .g = 0.85F, .b = 0.7F}, .lux = 75.0F});

    const auto assets = wulfhilde::load_map_assets(*root, *map);
    REQUIRE(assets.has_value());
    const auto& emission =
        assets->materials.at(static_cast<std::size_t>(lamp - map->materials().begin())).emission;
    REQUIRE(emission.has_value());
    CHECK(emission->nits == 400.0F);
}
