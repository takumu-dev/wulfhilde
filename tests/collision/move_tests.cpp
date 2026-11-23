// Bodies moved through one sector, which its walls stop, its floor holds up and its ceiling holds
// down.
#include "collision_printing.hpp" // IWYU pragma: keep
#include "maps.hpp"
#include <wulfhilde/collision/collision.hpp>
#include <wulfhilde/collision/ground.hpp>
#include <wulfhilde/collision/movement.hpp>
#include <wulfhilde/collision/pose.hpp>
#include <wulfhilde/core/angle.hpp>
#include <wulfhilde/core/rigid_transform.hpp>
#include <wulfhilde/core/rotation.hpp>
#include <wulfhilde/core/vector3.hpp>
#include <wulfhilde/world/ids.hpp>
#include <wulfhilde/world/map.hpp>
#include <wulfhilde/world/surface_ref.hpp>

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <optional>

using wulfhilde::Angle;
using wulfhilde::Ground;
using wulfhilde::Map;
using wulfhilde::Movement;
using wulfhilde::RigidTransform;
using wulfhilde::Rotation;
using wulfhilde::SectorCeiling;
using wulfhilde::SectorFloor;
using wulfhilde::SectorId;
using wulfhilde::Vector3;

namespace {

constexpr SectorId hall{0};

// How near a body ends to where the geometry puts it.
constexpr float tolerance = 1e-4F;

// Moves the person from a centre in the hall, upright with its sector's gravity. In one sector,
// the move must end there, having crossed no portal and taken no step.
Movement move_in_hall(const Map& map, const Vector3& centre, const Vector3& velocity,
                      float seconds) {
    const auto movement = wulfhilde::move(
        map, person, {.centre = {.sector = hall, .point = centre}}, velocity, seconds);
    CHECK(movement.pose.centre.sector == hall);
    CHECK(approx_equal(movement.transform, RigidTransform::identity(), Angle{}, 0.0F));
    CHECK(movement.stepped == 0.0F);
    return movement;
}

} // namespace

TEST_CASE("a body moving into a wall stops against it", "[collision]") {
    const auto map = hall_map();
    const auto movement = move_in_hall(map, {.x = 2.0F, .y = 2.0F, .z = 1.5F}, {.x = 3.0F}, 1.0F);
    CHECK(approx_equal(movement.pose.centre.point, {.x = 3.7F, .y = 2.0F, .z = 1.5F}, tolerance));
    CHECK(approx_equal(movement.velocity, Vector3{}, tolerance));
}

TEST_CASE("a body moving fast into a wall stops against it rather than passing through",
          "[collision]") {
    const auto map = hall_map();
    const auto movement = move_in_hall(map, {.x = 2.0F, .y = 2.0F, .z = 1.5F}, {.x = 50.0F}, 1.0F);
    CHECK(approx_equal(movement.pose.centre.point, {.x = 3.7F, .y = 2.0F, .z = 1.5F}, tolerance));
}

TEST_CASE("a body moving slantwise into a wall slides along it", "[collision]") {
    const auto map = hall_map();
    const auto movement =
        move_in_hall(map, {.x = 3.5F, .y = 1.0F, .z = 1.5F}, {.x = 1.0F, .y = 1.0F}, 1.0F);
    CHECK(approx_equal(movement.pose.centre.point, {.x = 3.7F, .y = 2.0F, .z = 1.5F}, tolerance));
    CHECK(approx_equal(movement.velocity, {.y = 1.0F}, tolerance));
}

TEST_CASE("a body moving into a corner stops in it", "[collision]") {
    const auto map = hall_map();
    const auto movement =
        move_in_hall(map, {.x = 3.0F, .y = 3.5F, .z = 1.5F}, {.x = 2.0F, .y = 1.0F}, 1.0F);
    CHECK(approx_equal(movement.pose.centre.point, {.x = 3.7F, .y = 3.7F, .z = 1.5F}, tolerance));
    CHECK(approx_equal(movement.velocity, Vector3{}, tolerance));
}

TEST_CASE("a body standing on the floor stays there, with the floor as its ground", "[collision]") {
    const auto map = hall_map();
    const auto movement = move_in_hall(map, {.x = 2.0F, .y = 2.0F, .z = 0.9F}, {.z = -1.0F}, 0.1F);
    CHECK(approx_equal(movement.pose.centre.point, {.x = 2.0F, .y = 2.0F, .z = 0.9F}, tolerance));
    CHECK(approx_equal(movement.velocity, Vector3{}, tolerance));
    CHECK(movement.ground == Ground{.surface = SectorFloor{hall}, .normal = {.z = 1.0F}});
}

TEST_CASE("a falling body lands on the floor and stands on it", "[collision]") {
    const auto map = hall_map();
    const auto movement =
        move_in_hall(map, {.x = 2.0F, .y = 2.0F, .z = 2.0F}, {.x = 0.5F, .z = -5.0F}, 1.0F);
    CHECK(approx_equal(movement.pose.centre.point, {.x = 2.5F, .y = 2.0F, .z = 0.9F}, tolerance));
    CHECK(approx_equal(movement.velocity, {.x = 0.5F}, tolerance));
    CHECK(movement.ground == Ground{.surface = SectorFloor{hall}, .normal = {.z = 1.0F}});
}

TEST_CASE("a body pressed onto the floor and into a wall runs along where they meet",
          "[collision]") {
    const auto map = hall_map();
    const auto movement = move_in_hall(map, {.x = 3.7F, .y = 1.0F, .z = 0.9F},
                                       {.x = 1.0F, .y = 1.0F, .z = -1.0F}, 1.0F);
    CHECK(approx_equal(movement.pose.centre.point, {.x = 3.7F, .y = 2.0F, .z = 0.9F}, tolerance));
    CHECK(approx_equal(movement.velocity, {.y = 1.0F}, tolerance));
    CHECK(movement.ground == Ground{.surface = SectorFloor{hall}, .normal = {.z = 1.0F}});
}

TEST_CASE("a body moving up stops against the ceiling, which isn't its ground", "[collision]") {
    const auto map = hall_map();
    const auto movement =
        move_in_hall(map, {.x = 2.0F, .y = 2.0F, .z = 1.5F}, {.x = 0.5F, .z = 3.0F}, 1.0F);
    CHECK(approx_equal(movement.pose.centre.point, {.x = 2.5F, .y = 2.0F, .z = 2.1F}, tolerance));
    CHECK(approx_equal(movement.velocity, {.x = 0.5F}, tolerance));
    CHECK(movement.ground == std::nullopt);
}

TEST_CASE("a body stands on whatever its gravity pulls it onto", "[collision]") {
    const auto map = hall_map();
    // Upside down, falling toward the ceiling.
    const auto movement =
        wulfhilde::move(map, person,
                        {
                            .centre = {.sector = hall, .point = {.x = 2.0F, .y = 2.0F, .z = 2.0F}},
                            .tilt = Rotation::about_axis({.x = 1.0F}, Angle::degrees(180.0F)),
                            .gravity_up = {.z = -1.0F},
                        },
                        {.z = 2.0F}, 1.0F);
    CHECK(approx_equal(movement.pose.centre.point, {.x = 2.0F, .y = 2.0F, .z = 2.1F}, tolerance));
    CHECK(movement.ground == Ground{.surface = SectorCeiling{hall}, .normal = {.z = -1.0F}});
}

TEST_CASE("a surface exactly as steep as a body's steepest slope is its ground", "[collision]") {
    const auto map = hall_map();
    auto shape = person;
    for (const auto degrees : {30.0F, 45.0F, 60.0F}) {
        CAPTURE(degrees);
        // The body's gravity up leans toward +X by the angle, so the floor is that steep for it.
        const auto lean = Rotation::about_axis({.y = 1.0F}, Angle::degrees(degrees));
        const wulfhilde::Pose leaning{
            .centre = {.sector = hall, .point = {.x = 2.0F, .y = 2.0F, .z = 0.9F}},
            .gravity_up = rotate(lean, {.z = 1.0F}),
        };

        shape.steepest_slope = Angle::degrees(degrees);
        CHECK(wulfhilde::move(map, shape, leaning, {}, 0.0F).ground ==
              Ground{.surface = SectorFloor{hall}, .normal = {.z = 1.0F}});

        shape.steepest_slope = Angle::degrees(degrees - 1.0F);
        CHECK(wulfhilde::move(map, shape, leaning, {}, 0.0F).ground == std::nullopt);
    }
}
