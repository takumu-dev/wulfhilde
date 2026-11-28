// The viewer's acceptance suite: the test maps in tests/content, loaded through the assets module
// as the viewer loads them.
#include "world_printing.hpp" // IWYU pragma: keep
#include <wulfhilde/assets/content_root.hpp>
#include <wulfhilde/assets/load_map.hpp>
#include <wulfhilde/assets/load_map_assets.hpp>
#include <wulfhilde/world/ids.hpp>
#include <wulfhilde/world/map.hpp>
#include <wulfhilde/world/sector.hpp>
#include <wulfhilde/world/sky.hpp>

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

TEST_CASE("the slopes map has a ramp hinged on a diagonal wall between two floor heights, a sloped "
          "ceiling, a floor and a ceiling that slope different ways, and slopes of 30 and 60 "
          "degrees",
          "[viewer]") {
    const auto root = wulfhilde::ContentRoot::open(WULFHILDE_TEST_CONTENT);
    REQUIRE(root.has_value());

    const auto map = wulfhilde::load_map(*root, "slopes");

    REQUIRE(map.has_value());
    CHECK(map->sector(map->start().location.sector).id == "hall");
    const auto slope_of = [&map](const wulfhilde::FloorOrCeiling& floor_or_ceiling) {
        REQUIRE(floor_or_ceiling.slope.has_value());
        const auto& hinge = map->wall(floor_or_ceiling.slope->hinge);
        return std::pair{hinge, floor_or_ceiling.slope->gradient};
    };

    // The ramp rises from the hall's floor, along its diagonal hinge, to the landing's.
    const auto& ramp = sector_named(*map, "ramp");
    const auto [ramp_hinge, ramp_gradient] = slope_of(ramp.floor);
    CHECK(ramp_hinge.start.x != ramp_hinge.end.x);
    CHECK(ramp_hinge.start.y != ramp_hinge.end.y);
    CHECK(ramp_hinge.link.has_value());
    const auto landing = map->find_sector("landing");
    REQUIRE(landing.has_value());
    const auto ramp_id = map->find_sector("ramp").value_or(wulfhilde::SectorId{0});
    CHECK(std::abs(map->floor_height({.sector = ramp_id, .point = {.x = 11.0F, .y = 7.0F}})) <
          1e-5F);
    CHECK(std::abs(map->floor_height({.sector = ramp_id, .point = {.x = 13.0F, .y = 9.0F}}) -
                   map->floor_height({.sector = landing.value_or(ramp_id)})) < 1e-5F);
    CHECK(!ramp.ceiling.slope.has_value());

    // The attic's ceiling slopes, over a flat floor.
    const auto& attic = sector_named(*map, "attic");
    CHECK(!attic.floor.slope.has_value());
    CHECK(slope_of(attic.ceiling).second < 0.0F);

    // The opposed sector's floor and ceiling are hinged on walls at right angles.
    const auto& opposed = sector_named(*map, "opposed");
    const auto [floor_hinge, floor_gradient] = slope_of(opposed.floor);
    const auto [ceiling_hinge, ceiling_gradient] = slope_of(opposed.ceiling);
    CHECK(dot(floor_hinge.end - floor_hinge.start, ceiling_hinge.end - ceiling_hinge.start) ==
          0.0F);
    CHECK(floor_gradient > 0.0F);
    CHECK(ceiling_gradient < 0.0F);

    // The gentle slope and the steep one, side by side, are 30 and 60 degrees steep.
    CHECK(std::abs(std::atan(slope_of(sector_named(*map, "gentle").floor).second) -
                   (std::numbers::pi_v<float> / 6.0F)) < 1e-6F);
    CHECK(std::abs(std::atan(slope_of(sector_named(*map, "steep").floor).second) -
                   (std::numbers::pi_v<float> / 3.0F)) < 1e-6F);

    const auto assets = wulfhilde::load_map_assets(*root, *map);
    REQUIRE(assets.has_value());
}
