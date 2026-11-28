// Bodies moved through transformed portals: carried into the far sector's coordinates, turned with
// the portal, and held by what is beyond it in its own coordinates.
#include "collision_printing.hpp" // IWYU pragma: keep
#include "maps.hpp"
#include <wulfhilde/collision/collision.hpp>
#include <wulfhilde/collision/movement.hpp>
#include <wulfhilde/collision/pose.hpp>
#include <wulfhilde/core/angle.hpp>
#include <wulfhilde/core/rigid_transform.hpp>
#include <wulfhilde/core/rotation.hpp>
#include <wulfhilde/core/vector3.hpp>
#include <wulfhilde/world/ids.hpp>

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <optional>

using wulfhilde::Angle;
using wulfhilde::Pose;
using wulfhilde::RigidTransform;
using wulfhilde::Rotation;
using wulfhilde::SectorId;
using wulfhilde::Vector3;

namespace {

constexpr SectorId hall{0};
constexpr SectorId gallery{1};

// How near a body ends to where the geometry puts it.
constexpr float tolerance = 1e-4F;

// How near a turned rotation ends to where the portal turns it.
constexpr auto turn_tolerance = Angle::degrees(1e-3F);

// The turned gallery's portal transform, from the hall's coordinates into the gallery's, with the
// gallery raised by a lift.
RigidTransform onto_gallery(float lift = 0.0F) {
    return {
        .rotation = Rotation::from_yaw_pitch_roll(Angle::degrees(90.0F), {}, {}),
        .translation = {.x = 24.0F, .z = lift},
    };
}

// An upright pose of a body in the hall, with its sector's gravity.
Pose in_hall(const Vector3& centre) { return {.centre = {.sector = hall, .point = centre}}; }

} // namespace

TEST_CASE("a body crossing a transformed portal ends in the far sector's coordinates, turned with "
          "the portal, its velocity turned too",
          "[collision]") {
    const auto map = turned_gallery_map();
    const auto movement =
        wulfhilde::move(map, person, in_hall({.x = 2.0F, .y = 3.5F, .z = 0.9F}), {.y = 1.0F}, 1.0F);

    // Its centre crosses the door at (2, 4), which lands at (20, 2), and walks on west.
    CHECK(movement.pose.centre.sector == gallery);
    CHECK(approx_equal(movement.pose.centre.point, {.x = 19.5F, .y = 2.0F, .z = 0.9F}, tolerance));
    CHECK(approx_equal(movement.velocity, {.x = -1.0F}, tolerance));
    CHECK(movement.transform == onto_gallery());
    CHECK(approx_equal(movement.pose.tilt, onto_gallery().rotation, turn_tolerance));
    CHECK(movement.pose.gravity_up == Vector3{.z = 1.0F});
    CHECK(movement.stepped == 0.0F);
}

TEST_CASE("a body crossing a portal that moves it far up ends at the far sector's height",
          "[collision]") {
    const auto map = turned_gallery_map(4.0F, 100.0F);
    const auto movement =
        wulfhilde::move(map, person, in_hall({.x = 2.0F, .y = 3.5F, .z = 0.9F}), {.y = 1.0F}, 1.0F);

    CHECK(movement.pose.centre.sector == gallery);
    CHECK(
        approx_equal(movement.pose.centre.point, {.x = 19.5F, .y = 2.0F, .z = 100.9F}, tolerance));
    CHECK(movement.transform == onto_gallery(100.0F));
    CHECK(movement.stepped == 0.0F);
}

TEST_CASE("a body walking back through a transformed portal returns by its inverse",
          "[collision]") {
    const auto map = turned_gallery_map();
    const Pose turned{
        .centre = {.sector = gallery, .point = {.x = 19.5F, .y = 2.0F, .z = 0.9F}},
        .tilt = onto_gallery().rotation,
    };
    const auto movement = wulfhilde::move(map, person, turned, {.x = 1.0F}, 1.0F);

    CHECK(movement.pose.centre.sector == hall);
    CHECK(approx_equal(movement.pose.centre.point, {.x = 2.0F, .y = 3.5F, .z = 0.9F}, tolerance));
    CHECK(approx_equal(movement.velocity, {.y = -1.0F}, tolerance));
    CHECK(movement.transform == inverse(onto_gallery()));
    CHECK(approx_equal(movement.pose.tilt, Rotation::identity(), turn_tolerance));
}

TEST_CASE("a body straddling a transformed portal is held by the far sector's walls, met in its "
          "own coordinates",
          "[collision]") {
    // The gallery is only 0.2 m deep, so the body stops with its front 0.2 m past the door.
    const auto map = turned_gallery_map(0.2F);
    const auto movement =
        wulfhilde::move(map, person, in_hall({.x = 2.0F, .y = 3.0F, .z = 0.9F}), {.y = 1.0F}, 2.0F);

    CHECK(movement.pose.centre.sector == hall);
    CHECK(approx_equal(movement.pose.centre.point, {.x = 2.0F, .y = 3.9F, .z = 0.9F}, 2e-3F));
    CHECK(approx_equal(movement.velocity, Vector3{}, tolerance));
    CHECK(movement.transform == RigidTransform::identity());
}

TEST_CASE("a body straddling a transformed portal settles where it is, in its centre's sector",
          "[collision]") {
    const auto map = turned_gallery_map();
    const auto pose = in_hall({.x = 2.0F, .y = 3.9F, .z = 0.9F});

    const auto settled = wulfhilde::settle(map, person, pose);

    CAPTURE(settled);
    CHECK(settled.transform([](const Pose& settled_pose) { return settled_pose.centre; }) ==
          std::optional(pose.centre));
}

TEST_CASE("a body crossing a transformed portal hands over the far sector's gravity",
          "[collision]") {
    const auto map = turned_gallery_map();
    // A body whose gravity a portal elsewhere tilted, still upright by it, and falling toward it.
    const auto gravity_up = normalize(Vector3{.y = -0.2F, .z = 1.0F});
    const Pose tilted{
        .centre = {.sector = hall, .point = {.x = 2.0F, .y = 3.5F, .z = 1.2F}},
        .tilt = Rotation::about_axis({.x = 1.0F}, angle_between({.z = 1.0F}, gravity_up)),
        .gravity_up = gravity_up,
    };
    const auto movement = wulfhilde::move(map, person, tilted, {.y = 1.0F}, 1.0F);

    REQUIRE(movement.pose.centre.sector == gallery);
    CHECK(movement.pose.gravity_up == Vector3{.z = 1.0F});
    // The body keeps its orientation, turned by the portal, and is left to right itself.
    CHECK(approx_equal(movement.pose.tilt, onto_gallery().rotation * tilted.tilt, turn_tolerance));
}

TEST_CASE("a body crossing an ordinary portal keeps its gravity", "[collision]") {
    const auto map = gallery_map();
    const auto gravity_up = normalize(Vector3{.y = -0.2F, .z = 1.0F});
    const Pose tilted{
        .centre = {.sector = hall, .point = {.x = 2.0F, .y = 3.5F, .z = 1.2F}},
        .tilt = Rotation::about_axis({.x = 1.0F}, angle_between({.z = 1.0F}, gravity_up)),
        .gravity_up = gravity_up,
    };
    const auto movement = wulfhilde::move(map, person, tilted, {.y = 1.0F}, 1.0F);

    REQUIRE(movement.pose.centre.sector == SectorId{1});
    CHECK(movement.pose.gravity_up == gravity_up);
    CHECK(movement.pose.tilt == tilted.tilt);
}
