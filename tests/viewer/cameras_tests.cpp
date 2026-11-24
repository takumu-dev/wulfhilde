// The cameras the viewer draws from, placed in the sectors test map, which the acceptance suite
// loads through the assets module as the viewer does.
#include "cameras.hpp"

#include "command_line.hpp"
#include "controller.hpp"
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

#include <array>
#include <expected>
#include <format>
#include <string_view>
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

TEST_CASE("the viewer draws from the eye, at a 60 degree field of view", "[viewer]") {
    const auto map = sectors_map();
    const viewer::Eye eye{
        .location =
            {
                .sector = map.start().location.sector,
                .point = {.x = 3.0F, .y = 1.0F, .z = 2.0F},
            },
        .facing = wulfhilde::Rotation::from_yaw_pitch_roll(Angle::degrees(30.0F),
                                                           Angle::degrees(-20.0F), Angle{}),
    };

    const auto camera = viewer::camera_at(eye);

    CHECK(camera.location == eye.location);
    check_facing(camera, Angle::degrees(30.0F), Angle::degrees(-20.0F), Angle{});
    CHECK(camera.vertical_field_of_view == Angle::degrees(60.0F));
}

TEST_CASE("the eye is written as --camera takes it, to the millimetre and the hundredth of a "
          "degree, with a roll only if it has one",
          "[viewer]") {
    const auto map = sectors_map();
    const auto hall = map.start().location.sector;

    const viewer::Eye upright{
        .location = {.sector = hall, .point = {.x = 2.0F, .y = 2.0F, .z = 1.65F}},
        .facing = wulfhilde::Rotation::identity(),
    };
    CHECK(viewer::camera_option(map, upright) == "hall,2,2,1.65,0,0");

    const viewer::Eye rolled{
        .location = {.sector = hall, .point = {.x = 11.23456F, .y = 0.0004F, .z = -0.5F}},
        .facing = wulfhilde::Rotation::from_yaw_pitch_roll(
            Angle::degrees(-165.5F), Angle::degrees(12.346F), Angle::degrees(-90.0F)),
    };
    CHECK(viewer::camera_option(map, rolled) == "hall,11.235,0,-0.5,-165.5,12.35,-90");
}

TEST_CASE("the eye written as --camera takes it is read back as the same camera", "[viewer]") {
    const auto map = sectors_map();
    const viewer::Eye eye{
        .location =
            {
                .sector = map.start().location.sector,
                .point = {.x = 7.25F, .y = 1.5F, .z = 1.65F},
            },
        .facing = wulfhilde::Rotation::from_yaw_pitch_roll(
            Angle::degrees(135.0F), Angle::degrees(-30.0F), Angle::degrees(45.0F)),
    };
    const auto written = viewer::camera_option(map, eye);
    const std::array<std::string_view, 3> arguments{"sectors.map.json", "--camera", written};

    const auto command_line = viewer::parse_command_line(arguments);
    REQUIRE(command_line.has_value());
    REQUIRE(command_line->camera.has_value());
    const auto camera =
        viewer::camera_from_option(map, command_line->camera.value_or(viewer::CameraOption{}));

    REQUIRE(camera.has_value());
    CHECK(camera->location == eye.location);
    CHECK(wulfhilde::approx_equal(camera->orientation, eye.facing, Angle::degrees(0.01F)));
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
