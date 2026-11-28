// Bodies against the walls of inner loops: sliding round a pillar, and stepping onto a dais or
// stopped at the rims of a sector raised too high, through inner loops of portals.
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

using wulfhilde::Ground;
using wulfhilde::Map;
using wulfhilde::Movement;
using wulfhilde::Pose;
using wulfhilde::SectorFloor;
using wulfhilde::SectorId;
using wulfhilde::Vector3;

namespace {

constexpr SectorId hall{0};
constexpr SectorId raised{1};

// How near a body ends to where the geometry puts it.
constexpr float tolerance = 1e-4F;

// An upright pose of a body in the hall, with its sector's gravity.
Pose in_hall(const Vector3& centre) { return {.centre = {.sector = hall, .point = centre}}; }

// Moves the person from a centre in the hall.
Movement move_from_hall(const Map& map, const Vector3& centre, const Vector3& velocity,
                        float seconds) {
    return wulfhilde::move(map, person, in_hall(centre), velocity, seconds);
}

} // namespace

TEST_CASE("a body walking into a pillar stops a radius short of it", "[collision]") {
    const auto movement =
        move_from_hall(pillar_map(), {.x = 1.0F, .y = 4.0F, .z = 0.9F}, {.x = 2.0F}, 2.0F);

    CHECK(movement.pose.centre.sector == hall);
    CHECK(approx_equal(movement.pose.centre.point, {.x = 2.7F, .y = 4.0F, .z = 0.9F}, tolerance));
    CHECK(approx_equal(movement.velocity, Vector3{}, tolerance));
    CHECK(movement.ground == Ground{.surface = SectorFloor{hall}, .normal = {.z = 1.0F}});
}

TEST_CASE("a body walking slantwise into a pillar slides along it", "[collision]") {
    // It meets the pillar's west wall, at x = 3, and slides north along it.
    const auto movement = move_from_hall(pillar_map(), {.x = 2.0F, .y = 3.5F, .z = 0.9F},
                                         {.x = 2.0F, .y = 0.5F}, 1.0F);

    CHECK(movement.pose.centre.sector == hall);
    CHECK(std::abs(movement.pose.centre.point.x - 2.7F) < tolerance);
    CHECK(movement.pose.centre.point.y > 3.9F);
    CHECK(approx_equal(movement.velocity, {.y = 0.5F}, tolerance));
}

TEST_CASE("a body walking past a pillar's corner slides round it", "[collision]") {
    // Heading north-east from south-west of the pillar, it meets the corner at (3, 3) off its line
    // and is turned along the pillar's south wall, so it ends east of the corner and south of the
    // pillar.
    const auto movement = move_from_hall(pillar_map(), {.x = 2.0F, .y = 2.2F, .z = 0.9F},
                                         {.x = 2.0F, .y = 1.0F}, 1.5F);

    CHECK(movement.pose.centre.sector == hall);
    const auto end = movement.pose.centre.point;
    CAPTURE(end);
    CHECK(end.x > 3.5F);
    CHECK(std::abs(end.y - 2.7F) < tolerance);
}

TEST_CASE("a body overlapping a pillar's corner settles out along the line from the corner",
          "[collision]") {
    const auto settled =
        wulfhilde::settle(pillar_map(), person, in_hall({.x = 2.82F, .y = 2.82F, .z = 0.9F}));

    const auto out = 0.3F / std::numbers::sqrt2_v<float>;
    CAPTURE(settled);
    CHECK((settled && approx_equal(settled->centre.point,
                                   {.x = 3.0F - out, .y = 3.0F - out, .z = 0.9F}, 1e-3F)));
}

TEST_CASE("a body walks up onto a dais through its inner loop's opening, and down off it",
          "[collision]") {
    // The dais is 0.3 m up, and the body steps 0.4 m.
    const auto map = dais_map(0.3F);

    const auto onto = move_from_hall(map, {.x = 2.0F, .y = 4.0F, .z = 0.9F}, {.x = 2.0F}, 1.0F);
    CHECK(onto.pose.centre.sector == raised);
    CHECK(approx_equal(onto.pose.centre.point, {.x = 4.0F, .y = 4.0F, .z = 1.2F}, tolerance));
    CHECK(std::abs(onto.stepped - 0.3F) < tolerance);
    CHECK(onto.ground == Ground{.surface = SectorFloor{raised}, .normal = {.z = 1.0F}});

    const auto off = wulfhilde::move(map, person, onto.pose, {.x = 2.0F}, 1.0F);
    CHECK(off.pose.centre.sector == hall);
    CHECK(approx_equal(off.pose.centre.point, {.x = 6.0F, .y = 4.0F, .z = 0.9F}, tolerance));
    CHECK(std::abs(off.stepped + 0.3F) < tolerance);
    CHECK(off.ground == Ground{.surface = SectorFloor{hall}, .normal = {.z = 1.0F}});
}

TEST_CASE("a dais higher than the body's step height stops it at its rim", "[collision]") {
    const auto movement =
        move_from_hall(dais_map(0.6F), {.x = 1.0F, .y = 4.0F, .z = 0.9F}, {.x = 2.0F}, 2.0F);

    CHECK(movement.pose.centre.sector == hall);
    CHECK(approx_equal(movement.pose.centre.point, {.x = 2.7F, .y = 4.0F, .z = 0.9F}, tolerance));
    CHECK(approx_equal(movement.velocity, Vector3{}, tolerance));
    CHECK(movement.stepped == 0.0F);
}

TEST_CASE("at a corner where two portals lead into the same sector raised too high, both rims stop "
          "the body",
          "[collision]") {
    // An L of a sector 1 m up, whose notch faces north-east with its corner at (4, 4). The hall's
    // walls on either side of the notch, at x = 4 and at y = 4, are sides of two portals into it.
    const auto map = raised_map(1.0F, {
                                          {.x = 3.0F, .y = 3.0F},
                                          {.x = 3.0F, .y = 6.0F},
                                          {.x = 4.0F, .y = 6.0F},
                                          {.x = 4.0F, .y = 4.0F},
                                          {.x = 6.0F, .y = 4.0F},
                                          {.x = 6.0F, .y = 3.0F},
                                      });

    // Walking south-west into the notch's corner from either side of its diagonal, the body ends
    // in the corner, a radius from both rims.
    for (const auto& velocity :
         {Vector3{.x = -2.0F, .y = -1.0F}, Vector3{.x = -1.0F, .y = -2.0F}}) {
        CAPTURE(velocity);
        const auto movement =
            move_from_hall(map, {.x = 5.0F, .y = 5.0F, .z = 0.9F}, velocity, 2.0F);

        CAPTURE(movement.pose.centre.point);
        CHECK(movement.pose.centre.sector == hall);
        CHECK(
            approx_equal(movement.pose.centre.point, {.x = 4.3F, .y = 4.3F, .z = 0.9F}, tolerance));
        CHECK(movement.stepped == 0.0F);
    }
}
