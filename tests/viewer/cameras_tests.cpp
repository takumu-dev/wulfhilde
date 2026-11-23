// The cameras the viewer draws from, placed in the sectors test map, which the acceptance suite
// loads through the assets module as the viewer does.
#include "cameras.hpp"

#include "command_line.hpp"
#include "math_printing.hpp"  // IWYU pragma: keep
#include "world_printing.hpp" // IWYU pragma: keep
#include <wulfhilde/assets/content_root.hpp>
#include <wulfhilde/assets/load_map.hpp>
#include <wulfhilde/core/angle.hpp>
#include <wulfhilde/core/rotation.hpp>
#include <wulfhilde/core/vector3.hpp>
#include <wulfhilde/renderer/camera.hpp>
#include <wulfhilde/world/map.hpp>

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <expected>
#include <format>
#include <utility>

namespace {

using wulfhilde::Angle;

wulfhilde::Map sectors_map() {
    const auto root = wulfhilde::ContentRoot::open(WULFHILDE_TEST_CONTENT);
    REQUIRE(root.has_value());
    auto map = wulfhilde::load_map(*root, "sectors");
    if (!map) {
        FAIL(std::format("{}", map.error()));
    }
    return *std::move(map);
}

// Checks that a camera faces as yaw, pitch and roll turn it, exactly.
void check_facing(const wulfhilde::Camera& camera, Angle yaw, Angle pitch, Angle roll) {
    const auto expected = wulfhilde::Rotation::from_yaw_pitch_roll(yaw, pitch, roll);
    CHECK(camera.orientation.forward() == expected.forward());
    CHECK(camera.orientation.up() == expected.up());
}

} // namespace

TEST_CASE("the viewer draws from 1.65 m above the start position's feet, at a 60 degree field "
          "of view",
          "[viewer]") {
    const auto map = sectors_map();

    const auto camera = viewer::camera_at_start(map);

    CHECK(camera.location.sector == map.start().location.sector);
    CHECK(camera.location.point == wulfhilde::Vector3{.x = 2.0F, .y = 2.0F, .z = 1.65F});
    check_facing(camera, map.start().yaw, map.start().pitch, Angle::degrees(0.0F));
    CHECK(camera.vertical_field_of_view == Angle::degrees(60.0F));
}

TEST_CASE("a --camera is placed in its sector of the map, at a 60 degree field of view",
          "[viewer]") {
    const auto map = sectors_map();

    const auto camera =
        viewer::camera_from_option(map, {
                                            .sector = "hall",
                                            .point = {.x = 11.0F, .y = 1.0F, .z = 1.65F},
                                            .yaw = Angle::degrees(165.0F),
                                            .pitch = Angle::degrees(-5.0F),
                                            .roll = Angle::degrees(10.0F),
                                        });

    REQUIRE(camera.has_value());
    CHECK(camera->location.sector == map.find_sector("hall"));
    CHECK(camera->location.point == wulfhilde::Vector3{.x = 11.0F, .y = 1.0F, .z = 1.65F});
    check_facing(*camera, Angle::degrees(165.0F), Angle::degrees(-5.0F), Angle::degrees(10.0F));
    CHECK(camera->vertical_field_of_view == Angle::degrees(60.0F));
}

TEST_CASE("a --camera in a sector the map doesn't have is a usage error", "[viewer]") {
    const auto map = sectors_map();

    const auto camera = viewer::camera_from_option(
        map, {.sector = "yard", .point = {.x = 2.0F, .y = 2.0F, .z = 1.0F}});

    REQUIRE(!camera.has_value());
    CHECK(camera.error().code == viewer::UsageErrorCode::camera_outside_map);
    CHECK(std::format("{}", camera.error()) == "--camera names no sector of the map: yard");
}

TEST_CASE("a --camera point outside its sector is a usage error", "[viewer]") {
    const auto map = sectors_map();

    for (const auto point : {
             wulfhilde::Vector3{.x = 8.0F, .y = 8.0F, .z = 1.0F},
             wulfhilde::Vector3{.x = 2.0F, .y = 2.0F, .z = -0.5F},
         }) {
        CAPTURE(point);

        const auto camera = viewer::camera_from_option(map, {.sector = "hall", .point = point});

        REQUIRE(!camera.has_value());
        CHECK(camera.error().code == viewer::UsageErrorCode::camera_outside_map);
        CHECK(std::format("{}", camera.error()) ==
              std::format("--camera's point, {},{},{}, isn't in sector hall", point.x, point.y,
                          point.z));
    }
}
