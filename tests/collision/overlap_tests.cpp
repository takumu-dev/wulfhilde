// Bodies in a sector that shares space with another, which only that sector's walls hold.
#include "collision_printing.hpp" // IWYU pragma: keep
#include "maps.hpp"
#include <wulfhilde/collision/collision.hpp>
#include <wulfhilde/collision/movement.hpp>
#include <wulfhilde/collision/pose.hpp>
#include <wulfhilde/core/vector3.hpp>
#include <wulfhilde/world/ids.hpp>
#include <wulfhilde/world/map.hpp>
#include <wulfhilde/world/map_description.hpp>

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <format>
#include <optional>
#include <utility>

using wulfhilde::Pose;
using wulfhilde::SectorId;
using wulfhilde::Vector3;

namespace {

constexpr SectorId hall{1};

// How near a body ends to where the geometry puts it.
constexpr float tolerance = 1e-4F;

// The yard, a square 10 m on a side from (0, 0) to (10, 10), with a notch in its north side from
// x = 4 to 6, down to y = 6, where a hut stands. A door 1 m wide in the hut's south face leads into
// the hall, a room from (2, 6) to (8, 10), bigger than the hut, so the yard's walls about the notch
// run through it at x = 4 and 6. Both are 3 m high.
wulfhilde::Map hut_map() {
    auto yard = sector_through("yard", 0.0F, 3.0F,
                               {
                                   {.x = 0.0F, .y = 0.0F},
                                   {.x = 10.0F, .y = 0.0F},
                                   {.x = 10.0F, .y = 10.0F},
                                   {.x = 6.0F, .y = 10.0F},
                                   {.x = 6.0F, .y = 6.0F},
                                   {.x = 5.5F, .y = 6.0F},
                                   {.x = 4.5F, .y = 6.0F},
                                   {.x = 4.0F, .y = 6.0F},
                                   {.x = 4.0F, .y = 10.0F},
                                   {.x = 0.0F, .y = 10.0F},
                               });
    yard.loops.front().at(5).id = "yard-door";
    auto room = sector_through("hall", 0.0F, 3.0F,
                               {
                                   {.x = 2.0F, .y = 6.0F},
                                   {.x = 4.5F, .y = 6.0F},
                                   {.x = 5.5F, .y = 6.0F},
                                   {.x = 8.0F, .y = 6.0F},
                                   {.x = 8.0F, .y = 10.0F},
                                   {.x = 2.0F, .y = 10.0F},
                               });
    room.loops.front().at(1).id = "hall-door";

    wulfhilde::MapDescription description{
        .sectors = {std::move(yard), std::move(room)},
        .portals = {{.a = {.id = "yard-door"}, .b = {.id = "hall-door"}}},
        .start = {.sector = "yard", .point = {.x = 5.0F, .y = 2.0F}},
    };
    auto map = wulfhilde::Map::create(std::move(description));
    if (!map) {
        FAIL(std::format("the description has problems:\n{}", map.error()));
    }
    return *std::move(map);
}

// An upright pose of a body in the hall, with its sector's gravity.
Pose in_hall(const Vector3& centre) { return {.centre = {.sector = hall, .point = centre}}; }

} // namespace

TEST_CASE("a body in the hall settles astride the line of the yard's wall, which isn't the hall's",
          "[collision]") {
    const auto map = hut_map();
    const auto pose = in_hall({.x = 4.0F, .y = 8.0F, .z = 0.9F});

    const auto settled = wulfhilde::settle(map, person, pose);

    CAPTURE(settled);
    CHECK((settled.has_value() && settled->centre.sector == hall &&
           approx_equal(settled->centre.point, pose.centre.point, tolerance)));
}

TEST_CASE("a body walks across the hall untouched by the yard's walls that run through it",
          "[collision]") {
    const auto map = hut_map();

    // Across both of the yard's walls about the notch, at x = 4 and 6.
    const auto movement =
        wulfhilde::move(map, person, in_hall({.x = 3.0F, .y = 8.0F, .z = 0.9F}), {.x = 4.0F}, 1.0F);

    CAPTURE(movement.pose.centre);
    CHECK(movement.pose.centre.sector == hall);
    CHECK(approx_equal(movement.pose.centre.point, {.x = 7.0F, .y = 8.0F, .z = 0.9F}, tolerance));
    CHECK(approx_equal(movement.velocity, {.x = 4.0F}, tolerance));
}
