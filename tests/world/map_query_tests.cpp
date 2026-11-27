// What a map answers about a location: whether it is in its sector, and the heights there.
#include "descriptions.hpp"
#include "world_printing.hpp" // IWYU pragma: keep
#include <wulfhilde/core/vector3.hpp>
#include <wulfhilde/world/ids.hpp>
#include <wulfhilde/world/location.hpp>
#include <wulfhilde/world/map.hpp>
#include <wulfhilde/world/opening.hpp>

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <optional>

using wulfhilde::Location;
using wulfhilde::Opening;
using wulfhilde::SectorId;
using wulfhilde::Vector3;
using wulfhilde::WallId;

namespace {

constexpr SectorId hall{0};

} // namespace

TEST_CASE("a point within its sector's walls, from its floor to its ceiling, is in the sector",
          "[world]") {
    const auto map = make_map(hall_map());
    for (const auto& inside : {
             Vector3{.x = 2.0F, .y = 2.0F, .z = 1.5F},
             Vector3{.x = 0.1F, .y = 3.9F, .z = 0.1F},
             Vector3{.x = 2.0F, .y = 2.0F, .z = 0.0F},
             Vector3{.x = 2.0F, .y = 2.0F, .z = 3.0F},
             Vector3{.x = 4.0F, .y = 2.0F, .z = 1.0F},
             Vector3{.x = 0.0F, .y = 0.0F, .z = 0.0F},
         }) {
        CAPTURE(inside);
        CHECK(map.contains({.sector = hall, .point = inside}));
    }
    for (const auto& outside : {
             Vector3{.x = 2.0F, .y = 2.0F, .z = -0.01F},
             Vector3{.x = 2.0F, .y = 2.0F, .z = 3.01F},
             Vector3{.x = 4.01F, .y = 2.0F, .z = 1.0F},
             Vector3{.x = 2.0F, .y = -0.01F, .z = 1.0F},
             Vector3{.x = -3.0F, .y = 2.0F, .z = 1.0F},
             Vector3{.x = 5.0F, .y = 5.0F, .z = 1.0F},
         }) {
        CAPTURE(outside);
        CHECK(!map.contains({.sector = hall, .point = outside}));
    }
}

TEST_CASE("a point in the notch of a concave sector isn't in the sector", "[world]") {
    const auto map = make_map(l_shaped_hall_map());
    CHECK(map.contains({.sector = hall, .point = {.x = 3.5F, .y = 0.5F, .z = 1.0F}}));
    CHECK(map.contains({.sector = hall, .point = {.x = 0.5F, .y = 3.5F, .z = 1.0F}}));
    CHECK(map.contains({.sector = hall, .point = {.x = 1.0F, .y = 1.0F, .z = 1.0F}}));
    CHECK(map.contains({.sector = hall, .point = {.x = 2.5F, .y = 1.0F, .z = 1.0F}}));
    CHECK(!map.contains({.sector = hall, .point = {.x = 3.0F, .y = 3.0F, .z = 1.0F}}));
    CHECK(!map.contains({.sector = hall, .point = {.x = 1.01F, .y = 1.01F, .z = 1.0F}}));
    CHECK(!map.contains({.sector = hall, .point = {.x = 4.0F, .y = 4.0F, .z = 1.0F}}));
}

TEST_CASE("one point can be in two overlapping sectors, so a location names its sector",
          "[world]") {
    auto description = hall_map();
    description.sectors.push_back(sector_through("hut", {
                                                            {.x = 1.0F, .y = 1.0F},
                                                            {.x = 6.0F, .y = 1.0F},
                                                            {.x = 6.0F, .y = 2.0F},
                                                            {.x = 1.0F, .y = 2.0F},
                                                        }));
    const auto map = make_map(description);
    constexpr SectorId hut{1};
    constexpr Vector3 in_both{.x = 3.0F, .y = 1.5F, .z = 1.0F};
    constexpr Vector3 in_hut_only{.x = 5.0F, .y = 1.5F, .z = 1.0F};
    constexpr Vector3 in_hall_only{.x = 3.0F, .y = 3.0F, .z = 1.0F};
    CHECK(map.contains({.sector = hall, .point = in_both}));
    CHECK(map.contains({.sector = hut, .point = in_both}));
    CHECK(map.contains({.sector = hut, .point = in_hut_only}));
    CHECK(!map.contains({.sector = hall, .point = in_hut_only}));
    CHECK(map.contains({.sector = hall, .point = in_hall_only}));
    CHECK(!map.contains({.sector = hut, .point = in_hall_only}));
}

TEST_CASE("a flat floor and ceiling are at their heights wherever they are asked about",
          "[world]") {
    auto description = hall_map();
    description.sectors.at(0).floor.height = -1.25F;
    description.sectors.at(0).ceiling.height = 2.75F;
    const auto map = make_map(description);
    for (const auto& point : {
             Vector3{.x = 2.0F, .y = 2.0F, .z = 1.0F},
             Vector3{.x = 0.0F, .y = 4.0F, .z = 0.0F},
             Vector3{.x = -7.0F, .y = 30.0F, .z = 100.0F},
         }) {
        const Location location{.sector = hall, .point = point};
        CAPTURE(location);
        CHECK(map.floor_height(location) == -1.25F);
        CHECK(map.ceiling_height(location) == 2.75F);
    }
}

TEST_CASE("a wall portal's opening runs from the higher floor to the lower ceiling, on both sides",
          "[world]") {
    auto description = doorway_map();
    description.sectors.at(1).floor.height = 0.5F;
    description.sectors.at(1).ceiling.height = 2.5F;
    const auto map = make_map(description);

    CHECK(map.opening(WallId{3}) == Opening{.bottom = 0.5F, .top = 2.5F});
    CHECK(map.opening(WallId{7}) == Opening{.bottom = 0.5F, .top = 2.5F});
}

TEST_CASE("an opening between sectors of the same heights spans the whole wall", "[world]") {
    const auto map = make_map(doorway_map());
    CHECK(map.opening(WallId{3}) == Opening{.bottom = 0.0F, .top = 3.0F});
}

TEST_CASE("an opening between sectors whose spans don't overlap is empty, at the near sector's "
          "nearest height",
          "[world]") {
    auto description = doorway_map();
    description.sectors.at(1).floor.height = 3.5F;
    description.sectors.at(1).ceiling.height = 6.0F;
    const auto map = make_map(description);

    CHECK(map.opening(WallId{3}) == Opening{.bottom = 3.0F, .top = 3.0F});
    CHECK(map.opening(WallId{7}) == Opening{.bottom = 3.5F, .top = 3.5F});
}

TEST_CASE("a solid wall has no opening", "[world]") {
    const auto map = make_map(doorway_map());
    for (const auto solid : {WallId{0}, WallId{2}, WallId{4}, WallId{8}}) {
        CAPTURE(solid);
        CHECK(map.opening(solid) == std::nullopt);
    }
}
