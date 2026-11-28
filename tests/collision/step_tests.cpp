// Bodies stepping up and down through wall portals between floors at different heights, and
// stopped by steps too high and openings too low for them.
#include "collision_printing.hpp" // IWYU pragma: keep
#include "maps.hpp"
#include <wulfhilde/collision/collision.hpp>
#include <wulfhilde/collision/ground.hpp>
#include <wulfhilde/collision/movement.hpp>
#include <wulfhilde/collision/pose.hpp>
#include <wulfhilde/core/vector3.hpp>
#include <wulfhilde/world/ids.hpp>
#include <wulfhilde/world/map.hpp>
#include <wulfhilde/world/surface_ref.hpp>

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <numbers>
#include <optional>

using wulfhilde::Ground;
using wulfhilde::Map;
using wulfhilde::Movement;
using wulfhilde::Pose;
using wulfhilde::SectorFloor;
using wulfhilde::SectorId;
using wulfhilde::Vector3;

namespace {

constexpr SectorId hall{0};
constexpr SectorId beyond{1};

// How near a body ends to where the geometry puts it.
constexpr float tolerance = 1e-4F;

// An upright pose of a body in the hall, with its sector's gravity.
Pose in_hall(const Vector3& centre) { return {.centre = {.sector = hall, .point = centre}}; }

// Moves the person from a centre in the hall.
Movement move_from_hall(const Map& map, const Vector3& centre, const Vector3& velocity,
                        float seconds) {
    return wulfhilde::move(map, person, in_hall(centre), velocity, seconds);
}

// Checks that a body walking north from the middle of the hall at 2 m/s for 2 s was stopped
// against the doorway's plane, a radius short of it, still on the hall's floor.
void check_stopped_at_doorway(const Map& map) {
    const auto movement = move_from_hall(map, {.x = 2.0F, .y = 2.0F, .z = 0.9F}, {.y = 2.0F}, 2.0F);

    CHECK(movement.pose.centre.sector == hall);
    CHECK(approx_equal(movement.pose.centre.point, {.x = 2.0F, .y = 3.7F, .z = 0.9F}, tolerance));
    CHECK(approx_equal(movement.velocity, Vector3{}, tolerance));
    CHECK(movement.stepped == 0.0F);
    CHECK(movement.ground == Ground{.surface = SectorFloor{hall}, .normal = {.z = 1.0F}});
}

// Checks that a body walking north from the middle of the hall at 2 m/s for 2 s was stopped by an
// opening lower than it, under a ceiling beyond at a height. The edge of the ceiling beyond, at
// most 0.3 m above the top of the body's axis, stops it within a radius of the opening.
void check_stopped_below_opening(float ceiling) {
    CAPTURE(ceiling);
    const auto movement = move_from_hall(corridor_beyond_map(0.0F, ceiling),
                                         {.x = 2.0F, .y = 2.0F, .z = 0.9F}, {.y = 2.0F}, 2.0F);

    CHECK(movement.pose.centre.sector == hall);
    CHECK(movement.pose.centre.point.y > 3.7F - tolerance);
    CHECK(movement.pose.centre.point.y < 4.0F);
    CHECK(std::abs(movement.pose.centre.point.z - 0.9F) < tolerance);
    CHECK(approx_equal(movement.velocity, Vector3{}, tolerance));
}

} // namespace

TEST_CASE("a body walks up a step no higher than its step height, lifted at once as its centre "
          "crosses the opening",
          "[collision]") {
    // The corridor beyond is 0.3 m up, and the body steps 0.4 m.
    const auto map = corridor_beyond_map(0.3F, 3.3F);

    // Until its centre crosses, the hall's floor holds the body, though it reaches over the step.
    const auto straddling =
        move_from_hall(map, {.x = 2.0F, .y = 3.5F, .z = 0.9F}, {.y = 2.0F}, 0.2F);
    CHECK(straddling.pose.centre.sector == hall);
    CHECK(approx_equal(straddling.pose.centre.point, {.x = 2.0F, .y = 3.9F, .z = 0.9F}, tolerance));
    CHECK(straddling.stepped == 0.0F);
    CHECK(straddling.ground == Ground{.surface = SectorFloor{hall}, .normal = {.z = 1.0F}});

    // The tick its centre crosses, the corridor's floor lifts it.
    const auto crossed = move_from_hall(map, {.x = 2.0F, .y = 3.9F, .z = 0.9F}, {.y = 2.0F}, 0.1F);
    CHECK(crossed.pose.centre.sector == beyond);
    CHECK(approx_equal(crossed.pose.centre.point, {.x = 2.0F, .y = 4.1F, .z = 1.2F}, tolerance));
    CHECK(approx_equal(crossed.velocity, {.y = 2.0F}, tolerance));
    CHECK(std::abs(crossed.stepped - 0.3F) < tolerance);
    CHECK(crossed.ground == Ground{.surface = SectorFloor{beyond}, .normal = {.z = 1.0F}});
}

TEST_CASE("a step exactly as high as the body's step height is one it walks up", "[collision]") {
    const auto map = corridor_beyond_map(0.4F, 3.4F);
    const auto movement = move_from_hall(map, {.x = 2.0F, .y = 2.0F, .z = 0.9F}, {.y = 2.0F}, 2.0F);

    CHECK(movement.pose.centre.sector == beyond);
    CHECK(approx_equal(movement.pose.centre.point, {.x = 2.0F, .y = 6.0F, .z = 1.3F}, tolerance));
    CHECK(std::abs(movement.stepped - 0.4F) < tolerance);
}

TEST_CASE("a step higher than the body's step height stops it, as a wall does", "[collision]") {
    check_stopped_at_doorway(corridor_beyond_map(0.6F, 3.6F));
}

TEST_CASE("a grounded body walking down a step no deeper than its step height snaps down onto the "
          "floor below",
          "[collision]") {
    const auto map = corridor_beyond_map(-0.3F, 3.0F);

    const auto crossed = move_from_hall(map, {.x = 2.0F, .y = 3.9F, .z = 0.9F}, {.y = 2.0F}, 0.1F);
    CHECK(crossed.pose.centre.sector == beyond);
    CHECK(approx_equal(crossed.pose.centre.point, {.x = 2.0F, .y = 4.1F, .z = 0.6F}, tolerance));
    CHECK(approx_equal(crossed.velocity, {.y = 2.0F}, tolerance));
    CHECK(std::abs(crossed.stepped + 0.3F) < tolerance);
    CHECK(crossed.ground == Ground{.surface = SectorFloor{beyond}, .normal = {.z = 1.0F}});
}

TEST_CASE("a body snaps down only from ground, only onto a floor up to a step below, and not while "
          "rising",
          "[collision]") {
    // A body that stands on nothing walks on over the step down in the air.
    {
        const auto map = corridor_beyond_map(-0.3F, 3.0F);
        const auto movement =
            move_from_hall(map, {.x = 2.0F, .y = 3.9F, .z = 1.0F}, {.y = 2.0F}, 0.1F);
        CHECK(movement.pose.centre.sector == beyond);
        CHECK(
            approx_equal(movement.pose.centre.point, {.x = 2.0F, .y = 4.1F, .z = 1.0F}, tolerance));
        CHECK(movement.stepped == 0.0F);
        CHECK(movement.ground == std::nullopt);
    }
    // A drop deeper than the step leaves a grounded body in the air.
    {
        const auto map = corridor_beyond_map(-0.6F, 3.0F);
        const auto movement =
            move_from_hall(map, {.x = 2.0F, .y = 3.9F, .z = 0.9F}, {.y = 2.0F}, 0.1F);
        CHECK(movement.pose.centre.sector == beyond);
        CHECK(
            approx_equal(movement.pose.centre.point, {.x = 2.0F, .y = 4.1F, .z = 0.9F}, tolerance));
        CHECK(movement.stepped == 0.0F);
        CHECK(movement.ground == std::nullopt);
    }
    // A body jumping off the ground rises on.
    {
        const auto map = corridor_beyond_map(-0.3F, 3.0F);
        const auto movement =
            move_from_hall(map, {.x = 2.0F, .y = 3.9F, .z = 0.9F}, {.y = 2.0F, .z = 1.0F}, 0.1F);
        CHECK(
            approx_equal(movement.pose.centre.point, {.x = 2.0F, .y = 4.1F, .z = 1.0F}, tolerance));
        CHECK(movement.stepped == 0.0F);
        CHECK(movement.ground == std::nullopt);
    }
}

TEST_CASE("a step that leaves the body too little room below the ceiling beyond is refused",
          "[collision]") {
    // From the corridor's floor, 0.3 m up, to its ceiling is 1.7 m, short of the body's 1.8 m.
    check_stopped_at_doorway(corridor_beyond_map(0.3F, 2.0F));
}

TEST_CASE("a step that leaves the body exactly its height below the ceiling beyond is one it walks "
          "up",
          "[collision]") {
    const auto map = corridor_beyond_map(0.3F, 2.1F);
    const auto movement = move_from_hall(map, {.x = 2.0F, .y = 2.0F, .z = 0.9F}, {.y = 2.0F}, 2.0F);

    CHECK(movement.pose.centre.sector == beyond);
    CHECK(approx_equal(movement.pose.centre.point, {.x = 2.0F, .y = 6.0F, .z = 1.2F}, tolerance));
    CHECK(std::abs(movement.stepped - 0.3F) < tolerance);
}

TEST_CASE("a body pressed hard and slantwise into a refused step is stopped clear of it, not "
          "wedged into the wall above",
          "[collision]") {
    // Without asking whether the body fits above the step, it would take the step, and the ceiling
    // beyond would push it back down into the step's edge.
    const auto map = corridor_beyond_map(0.3F, 2.0F);
    const auto movement = move_from_hall(map, {.x = 1.5F, .y = 2.0F, .z = 0.9F},
                                         {.x = 4.0F, .y = 8.0F, .z = -1.0F}, 2.0F);

    CHECK(movement.pose.centre.sector == hall);
    CHECK(std::abs(movement.pose.centre.point.y - 3.7F) < tolerance);
    CHECK(std::abs(movement.pose.centre.point.z - 0.9F) < tolerance);
    CHECK(movement.stepped == 0.0F);
    // It fits where it stopped, with nothing to push it out of.
    const auto settled = wulfhilde::settle(map, person, movement.pose);
    CAPTURE(movement.pose, settled);
    CHECK((settled && approx_equal(settled->centre.point, movement.pose.centre.point, tolerance)));
}

TEST_CASE("an opening lower than the body's height stops it", "[collision]") {
    check_stopped_below_opening(1.6F);
    check_stopped_below_opening(1.75F);
}

TEST_CASE("an opening as tall as the body, or taller, lets it through", "[collision]") {
    for (const auto ceiling : {1.8F, 2.0F}) {
        CAPTURE(ceiling);
        const auto movement = move_from_hall(corridor_beyond_map(0.0F, ceiling),
                                             {.x = 2.0F, .y = 2.0F, .z = 0.9F}, {.y = 2.0F}, 2.0F);

        CHECK(movement.pose.centre.sector == beyond);
        CHECK(
            approx_equal(movement.pose.centre.point, {.x = 2.0F, .y = 6.0F, .z = 0.9F}, tolerance));
        CHECK(movement.stepped == 0.0F);
    }
}

TEST_CASE("a cap reaching past an opening meets the floor beyond at its edge, and is pushed off it "
          "from there",
          "[collision]") {
    // A floor beyond 1 m up, too high to step onto with 1.7 m below the ceiling beyond. The body's
    // lower cap, centred 0.1 m above the floor beyond and 0.1 m short of the opening, reaches past
    // it, and is pushed away from the floor's edge rather than straight up from its plane.
    const auto map = corridor_beyond_map(1.0F, 2.7F);
    constexpr Vector3 still{};
    const auto movement = move_from_hall(map, {.x = 2.0F, .y = 3.9F, .z = 1.7F}, still, 0.0F);

    const auto off = 0.3F / std::numbers::sqrt2_v<float>;
    CHECK(movement.pose.centre.sector == hall);
    CHECK(approx_equal(movement.pose.centre.point,
                       {.x = 2.0F, .y = 4.0F - off, .z = 1.0F + off + 0.6F}, tolerance));
    CHECK(movement.stepped == 0.0F);
}
