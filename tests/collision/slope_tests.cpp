// Bodies on sloped floors: standing on slopes up to their steepest, sliding down steeper ones, and
// meeting slopes beyond wall portals.
#include "collision_printing.hpp" // IWYU pragma: keep
#include "maps.hpp"
#include <wulfhilde/collision/collision.hpp>
#include <wulfhilde/collision/ground.hpp>
#include <wulfhilde/collision/movement.hpp>
#include <wulfhilde/collision/pose.hpp>
#include <wulfhilde/core/angle.hpp>
#include <wulfhilde/core/vector3.hpp>
#include <wulfhilde/world/ids.hpp>
#include <wulfhilde/world/map.hpp>
#include <wulfhilde/world/map_description.hpp>
#include <wulfhilde/world/surface_ref.hpp>

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <numbers>
#include <optional>
#include <utility>

using wulfhilde::Angle;
using wulfhilde::Ground;
using wulfhilde::Map;
using wulfhilde::Pose;
using wulfhilde::SectorFloor;
using wulfhilde::SectorId;
using wulfhilde::SlopeDescription;
using wulfhilde::Vector3;

namespace {

constexpr SectorId hall{0};
constexpr SectorId beyond{1};

// How near a body ends to where the geometry puts it.
constexpr float tolerance = 1e-3F;

// The gradients of slopes 30 and 60 degrees steep: their tangents.
constexpr float gentle = std::numbers::inv_sqrt3_v<float>;
constexpr float steep = std::numbers::sqrt3_v<float>;

// The velocity of a body that stays where it is.
constexpr Vector3 still{};

// A body's ground, or for a body that has none, a ceiling, which no check here expects.
Ground ground_of(const std::optional<Ground>& ground) {
    return ground.value_or(Ground{.surface = wulfhilde::SectorCeiling{hall}, .normal = {}});
}

// The hall, a square 4 m on a side from (0, 0) to (4, 4) under a ceiling 10 m up, its floor sloped
// up northward from its south wall at a gradient.
Map sloped_hall_map(float gradient) {
    auto hall_sector = sector_through("hall", 0.0F, 10.0F,
                                      {
                                          {.x = 0.0F, .y = 0.0F},
                                          {.x = 4.0F, .y = 0.0F},
                                          {.x = 4.0F, .y = 4.0F},
                                          {.x = 0.0F, .y = 4.0F},
                                      });
    hall_sector.loops.front().front().id = "hall-south";
    hall_sector.floor.slope = SlopeDescription{.hinge = "hall-south", .gradient = gradient};
    return one_sector_map(std::move(hall_sector));
}

// The direction a floor sloped up northward at a gradient faces.
Vector3 slope_normal(float gradient) { return normalize(Vector3{.y = -gradient, .z = 1.0F}); }

// An upright pose of the person standing on a floor sloped up northward from y = 0 at a gradient,
// in a sector, with the centre of its plan at a point. Its lower cap rests on the slope, a radius
// from it, square to it.
Pose standing_on_slope(SectorId sector, float gradient, float x, float y) {
    const auto lift = (person.radius * std::sqrt(1.0F + (gradient * gradient))) +
                      (person.height * 0.5F) - person.radius;
    return {.centre = {.sector = sector, .point = {.x = x, .y = y, .z = (gradient * y) + lift}}};
}

// The hall with a doorway into a corridor beyond, 2 m wide and 4 m long, from (1, 4) to (3, 8),
// whose floor slopes up northward from the doorway at a gradient, under a ceiling 10 m up.
Map slope_beyond_map(float gradient) {
    auto description =
        doorway_description(0.0F, 10.0F, {{.x = 3.0F, .y = 8.0F}, {.x = 1.0F, .y = 8.0F}});
    description.sectors.at(1).floor.slope =
        SlopeDescription{.hinge = "beyond-door", .gradient = gradient};
    return make_map(std::move(description));
}

} // namespace

TEST_CASE("a body stands on a sloped floor a radius from it, with the slope as its ground",
          "[collision]") {
    const auto map = sloped_hall_map(gentle);
    const auto pose = standing_on_slope(hall, gentle, 2.0F, 2.0F);

    const auto movement = wulfhilde::move(map, person, pose, still, 0.5F);

    CHECK(approx_equal(movement.pose.centre.point, pose.centre.point, tolerance));
    REQUIRE(movement.ground.has_value());
    const auto ground = ground_of(movement.ground);
    CHECK(ground.surface == wulfhilde::SurfaceRef{SectorFloor{hall}});
    CHECK(approx_equal(ground.normal, slope_normal(gentle), 1e-5F));
}

TEST_CASE("a slope exactly as steep as a body's steepest slope is its ground, and one any steeper "
          "isn't",
          "[collision]") {
    // The person's steepest slope is 45 degrees, a gradient of 1.
    for (const auto gradient : {0.5F, 1.0F}) {
        CAPTURE(gradient);
        const auto map = sloped_hall_map(gradient);
        const auto ground =
            wulfhilde::move(map, person, standing_on_slope(hall, gradient, 2.0F, 2.0F), still, 0.0F)
                .ground;
        CHECK(ground.has_value());
        CHECK(ground_of(ground).surface == wulfhilde::SurfaceRef{SectorFloor{hall}});
    }

    // Steeper by 0.1 degrees.
    const auto steeper = std::tan(Angle::degrees(45.1F).in_radians());
    const auto map = sloped_hall_map(steeper);
    CHECK(wulfhilde::move(map, person, standing_on_slope(hall, steeper, 2.0F, 2.0F), still, 0.0F)
              .ground == std::nullopt);
}

TEST_CASE("a body on a slope steeper than its steepest has no ground, and slides down it as it "
          "falls",
          "[collision]") {
    const auto map = sloped_hall_map(steep);
    const auto pose = standing_on_slope(hall, steep, 2.0F, 2.0F);

    const auto movement = wulfhilde::move(map, person, pose, {.z = -2.0F}, 0.25F);

    CHECK(movement.ground == std::nullopt);
    // Down the slope, toward its hinge, still resting on it.
    const auto end = movement.pose.centre.point;
    CAPTURE(end);
    CHECK(end.y < 2.0F - 0.1F);
    CHECK(approx_equal(movement.pose.centre.point,
                       standing_on_slope(hall, steep, 2.0F, end.y).centre.point, tolerance));
    CHECK(movement.velocity.y < 0.0F);
    CHECK(std::abs(dot(movement.velocity, slope_normal(steep))) < tolerance);
}

TEST_CASE("a grounded body walking into a slope too steep beyond an opening stops at its foot, "
          "without rising",
          "[collision]") {
    const auto map = slope_beyond_map(steep);

    const auto movement = wulfhilde::move(
        map, person, {.centre = {.sector = hall, .point = {.x = 2.0F, .y = 2.0F, .z = 0.9F}}},
        {.y = 2.0F}, 2.0F);

    CHECK(movement.pose.centre.sector == hall);
    const auto end = movement.pose.centre.point;
    CAPTURE(end);
    // The lower cap touches the slope beyond the opening before the centre reaches the door.
    CHECK(end.y > 3.7F);
    CHECK(end.y < 4.0F);
    CHECK(std::abs(end.z - 0.9F) < tolerance);
    CHECK(approx_equal(movement.velocity, still, tolerance));
    CHECK(movement.ground == Ground{.surface = SectorFloor{hall}, .normal = {.z = 1.0F}});
}

TEST_CASE("a body walks up a walkable slope beyond an opening, standing on it", "[collision]") {
    const auto map = slope_beyond_map(gentle);

    const auto movement = wulfhilde::move(
        map, person, {.centre = {.sector = hall, .point = {.x = 2.0F, .y = 2.0F, .z = 0.9F}}},
        {.y = 2.0F}, 2.0F);

    CHECK(movement.pose.centre.sector == beyond);
    const auto end = movement.pose.centre.point;
    CAPTURE(end);
    CHECK(end.y > 5.0F);
    // The slope rises from the door at y = 4.
    CHECK(approx_equal(end,
                       standing_on_slope(beyond, gentle, 2.0F, end.y - 4.0F).centre.point +
                           Vector3{.y = 4.0F},
                       tolerance));
    REQUIRE(movement.ground.has_value());
    const auto ground = ground_of(movement.ground);
    CHECK(ground.surface == wulfhilde::SurfaceRef{SectorFloor{beyond}});
    CHECK(approx_equal(ground.normal, slope_normal(gentle), 1e-5F));
}

TEST_CASE("a body at the foot of a walkable slope beyond an opening already stands on it",
          "[collision]") {
    const auto map = slope_beyond_map(gentle);

    // Its centre is short of the door, but its lower cap reaches over the slope.
    const auto movement = wulfhilde::move(
        map, person, {.centre = {.sector = hall, .point = {.x = 2.0F, .y = 3.95F, .z = 0.9F}}},
        still, 0.1F);

    CHECK(movement.pose.centre.sector == hall);
    CHECK(movement.pose.centre.point.z > 0.9F + tolerance);
    CHECK(movement.ground.has_value());
    CHECK(ground_of(movement.ground).surface == wulfhilde::SurfaceRef{SectorFloor{beyond}});
}
