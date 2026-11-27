// Bodies moved through wall portals: straddling an opening, held by what is beyond it, and
// crossing it.
#include "collision_printing.hpp" // IWYU pragma: keep
#include "maps.hpp"
#include <wulfhilde/collision/collision.hpp>
#include <wulfhilde/collision/ground.hpp>
#include <wulfhilde/collision/movement.hpp>
#include <wulfhilde/collision/pose.hpp>
#include <wulfhilde/core/angle.hpp>
#include <wulfhilde/core/rigid_transform.hpp>
#include <wulfhilde/core/vector3.hpp>
#include <wulfhilde/world/ids.hpp>
#include <wulfhilde/world/map.hpp>
#include <wulfhilde/world/surface_ref.hpp>

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <utility>

using wulfhilde::Angle;
using wulfhilde::Map;
using wulfhilde::Movement;
using wulfhilde::Pose;
using wulfhilde::RigidTransform;
using wulfhilde::SectorId;
using wulfhilde::Vector3;

namespace {

constexpr SectorId hall{0};
constexpr SectorId beyond{1};

// How near a body ends to where the geometry puts it.
constexpr float tolerance = 1e-4F;

// An upright pose of a body in the hall, with its sector's gravity.
Pose in_hall(const Vector3& centre) { return {.centre = {.sector = hall, .point = centre}}; }

// Moves the person from a centre in the hall, which through ordinary portals turns nothing.
Movement move_from_hall(const Map& map, const Vector3& centre, const Vector3& velocity,
                        float seconds) {
    const auto movement = wulfhilde::move(map, person, in_hall(centre), velocity, seconds);
    CHECK(approx_equal(movement.transform, RigidTransform::identity(), Angle{}, 0.0F));
    CHECK(movement.stepped == 0.0F);
    return movement;
}

} // namespace

TEST_CASE("a body straddling an opening settles where it is, in its centre's sector",
          "[collision]") {
    const auto map = gallery_map();
    const auto pose = in_hall({.x = 2.0F, .y = 3.9F, .z = 0.9F});

    const auto settled = wulfhilde::settle(map, person, pose);

    CAPTURE(settled);
    CHECK(settled.transform([](const Pose& settled_pose) { return settled_pose.centre; }) ==
          std::optional(pose.centre));
}

TEST_CASE("a body straddling an opening moves along it unhindered, staying in its centre's sector",
          "[collision]") {
    const auto map = gallery_map();
    const auto movement =
        move_from_hall(map, {.x = 1.5F, .y = 3.9F, .z = 0.9F}, {.x = 1.0F, .z = -1.0F}, 1.0F);

    CHECK(movement.pose.centre.sector == hall);
    CHECK(approx_equal(movement.pose.centre.point, {.x = 2.5F, .y = 3.9F, .z = 0.9F}, tolerance));
    CHECK(approx_equal(movement.velocity, {.x = 1.0F}, tolerance));
}

TEST_CASE("a body's sector changes when its centre crosses an opening, and not before",
          "[collision]") {
    const auto map = gallery_map();

    const auto straddling =
        move_from_hall(map, {.x = 2.0F, .y = 3.5F, .z = 0.9F}, {.y = 1.0F}, 0.4F);
    CHECK(straddling.pose.centre.sector == hall);
    CHECK(approx_equal(straddling.pose.centre.point, {.x = 2.0F, .y = 3.9F, .z = 0.9F}, tolerance));

    const auto crossed = move_from_hall(map, {.x = 2.0F, .y = 3.9F, .z = 0.9F}, {.y = 1.0F}, 0.2F);
    CHECK(crossed.pose.centre.sector == beyond);
    CHECK(approx_equal(crossed.pose.centre.point, {.x = 2.0F, .y = 4.1F, .z = 0.9F}, tolerance));
    CHECK(approx_equal(crossed.velocity, {.y = 1.0F}, tolerance));
}

TEST_CASE("a body walks through a doorway into the sector beyond and on to its far wall",
          "[collision]") {
    const auto map = gallery_map();
    const auto movement =
        move_from_hall(map, {.x = 2.0F, .y = 2.0F, .z = 0.9F}, {.y = 4.0F, .z = -1.0F}, 2.0F);

    CHECK(movement.pose.centre.sector == beyond);
    CHECK(approx_equal(movement.pose.centre.point, {.x = 2.0F, .y = 7.7F, .z = 0.9F}, tolerance));
    CHECK(approx_equal(movement.velocity, Vector3{}, tolerance));
    CHECK(movement.ground ==
          wulfhilde::Ground{.surface = wulfhilde::SectorFloor{beyond}, .normal = {.z = 1.0F}});
}

TEST_CASE("a body walking into the solid wall beside an opening is stopped by it", "[collision]") {
    const auto map = gallery_map();
    const auto movement = move_from_hall(map, {.x = 0.5F, .y = 2.0F, .z = 0.9F}, {.y = 2.0F}, 2.0F);

    CHECK(movement.pose.centre.sector == hall);
    CHECK(approx_equal(movement.pose.centre.point, {.x = 0.5F, .y = 3.7F, .z = 0.9F}, tolerance));
    CHECK(approx_equal(movement.velocity, Vector3{}, tolerance));
}

TEST_CASE("a wall beyond an opening stops a body that reaches it through the opening",
          "[collision]") {
    // An alcove 0.2 m deep, so that its back wall stops the body before its centre crosses.
    const auto map = doorway_map(0.0F, 3.0F, {{.x = 3.0F, .y = 4.2F}, {.x = 1.0F, .y = 4.2F}});
    const auto movement = move_from_hall(map, {.x = 2.0F, .y = 2.0F, .z = 0.9F}, {.y = 2.0F}, 2.0F);

    CHECK(movement.pose.centre.sector == hall);
    CHECK(approx_equal(movement.pose.centre.point, {.x = 2.0F, .y = 3.9F, .z = 0.9F}, tolerance));
    CHECK(approx_equal(movement.velocity, Vector3{}, tolerance));

    // Nor does a body fit there, pushed out by more than a third of its radius.
    CHECK(wulfhilde::settle(map, person, in_hall({.x = 2.0F, .y = 4.05F, .z = 0.9F})) ==
          std::nullopt);
}

TEST_CASE("a body that has crossed an opening slides along the walls of the sector beyond",
          "[collision]") {
    const auto map = corridor_beyond_map();
    const auto movement =
        move_from_hall(map, {.x = 2.0F, .y = 3.0F, .z = 0.9F}, {.x = 0.5F, .y = 2.0F}, 2.0F);

    CHECK(movement.pose.centre.sector == beyond);
    CHECK(approx_equal(movement.pose.centre.point, {.x = 2.7F, .y = 7.0F, .z = 0.9F}, tolerance));
    CHECK(approx_equal(movement.velocity, {.y = 2.0F}, tolerance));
}

TEST_CASE("a strip below or above an opening stops a body as a wall does", "[collision]") {
    // A floor beyond a metre higher leaves a lower strip from 0 to 1 m, and a ceiling beyond at
    // 1.2 m an upper strip from there to 3 m, across the body's 1.8 m.
    for (const auto& [floor, ceiling] : {std::pair{1.0F, 3.0F}, std::pair{0.0F, 1.2F}}) {
        CAPTURE(floor, ceiling);
        const auto map = corridor_beyond_map(floor, ceiling);
        const auto movement =
            move_from_hall(map, {.x = 2.0F, .y = 2.0F, .z = 0.9F}, {.y = 2.0F}, 2.0F);

        CHECK(movement.pose.centre.sector == hall);
        CHECK(
            approx_equal(movement.pose.centre.point, {.x = 2.0F, .y = 3.7F, .z = 0.9F}, tolerance));
        CHECK(approx_equal(movement.velocity, Vector3{}, tolerance));
    }
}
