// The controller, which turns the player's input into the movement of their body, ticked through
// collision on the sectors test map and on maps built in code.
#include "controller.hpp"

#include "collision_printing.hpp" // IWYU pragma: keep
#include "math_printing.hpp"      // IWYU pragma: keep
#include "scripted_walk.hpp"
#include "world_printing.hpp" // IWYU pragma: keep
#include <wulfhilde/core/angle.hpp>
#include <wulfhilde/core/rotation.hpp>
#include <wulfhilde/core/vector3.hpp>
#include <wulfhilde/world/map.hpp>
#include <wulfhilde/world/map_description.hpp>

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <format>
#include <numbers>
#include <utility>
#include <vector>

namespace {

using wulfhilde::Angle;
using wulfhilde::Vector3;

// A map of one square sector, 200 m on a side, centred on the origin, with its floor at 0 m and
// its ceiling at a height. The player starts at a point, facing along +X.
wulfhilde::Map square_map(float ceiling, const Vector3& start) {
    std::vector<wulfhilde::WallDescription> walls;
    for (const auto& [x, y] : {
             std::pair{-100.0F, -100.0F},
             std::pair{100.0F, -100.0F},
             std::pair{100.0F, 100.0F},
             std::pair{-100.0F, 100.0F},
         }) {
        walls.push_back({.start = {.x = x, .y = y}, .surface = {.material = "test/wall"}});
    }
    wulfhilde::MapDescription description{.start = {.sector = "square", .point = start}};
    description.sectors.push_back({
        .id = "square",
        .floor = {.height = 0.0F, .material = "test/floor"},
        .ceiling = {.height = ceiling, .material = "test/ceiling"},
        .loops = {std::move(walls)},
    });
    auto map = wulfhilde::Map::create(std::move(description));
    if (!map) {
        FAIL(std::format("the description has problems:\n{}", map.error()));
    }
    return *std::move(map);
}

// The square, 10 m high, where the player starts standing at its middle.
wulfhilde::Map open_square() { return square_map(10.0F, {}); }

// Ticks a controller for a time, holding the same actions.
void hold(viewer::Controller& controller, const wulfhilde::Map& map, const viewer::Actions& actions,
          float seconds) {
    for (std::size_t tick = 0; tick < ticks_in(seconds); ++tick) {
        controller.tick(map, actions);
    }
}

// Returns where the body's centre is.
Vector3 center_of(const viewer::Controller& controller) {
    const auto& body = controller.body();
    REQUIRE(body.has_value());
    return body ? body->centre.point : Vector3{};
}

// Returns how fast the body moves over one more tick of the actions, in metres per second.
Vector3 velocity_over_tick(viewer::Controller& controller, const wulfhilde::Map& map,
                           const viewer::Actions& actions) {
    const auto before = center_of(controller);
    controller.tick(map, actions);
    return (center_of(controller) - before) / viewer::Controller::tick_seconds;
}

// Checks that a facing is the one a yaw, a pitch and a roll make, to within float rounding.
void check_facing(const wulfhilde::Rotation& facing, Angle yaw, Angle pitch, Angle roll) {
    CHECK(wulfhilde::approx_equal(
        facing, wulfhilde::Rotation::from_yaw_pitch_roll(yaw, pitch, roll), Angle::degrees(0.01F)));
}

} // namespace

TEST_CASE("a body stands upright on the ground at the start position, with its eye 1.65 m above "
          "its feet",
          "[viewer]") {
    const auto map = load_test_map("sectors");

    const auto controller = viewer::Controller::at_start(map);

    const auto& body = controller.body();
    REQUIRE(body.has_value());
    CHECK((body && body->centre.sector == map.start().location.sector));
    CHECK(wulfhilde::approx_equal(center_of(controller), Vector3{.x = 2.0F, .y = 2.0F, .z = 0.9F},
                                  1e-4F));
    CHECK((body && body->gravity_up == Vector3{.z = 1.0F}));
    CHECK(controller.grounded());

    const auto eye = controller.eye(map);
    CHECK(eye.location.sector == map.start().location.sector);
    CHECK(wulfhilde::approx_equal(eye.location.point, Vector3{.x = 2.0F, .y = 2.0F, .z = 1.65F},
                                  1e-4F));
    check_facing(eye.facing, map.start().yaw, map.start().pitch, Angle{});
}

TEST_CASE("walking reaches 4 m/s along the facing in 0.1 s, accelerating at 40 m/s^2", "[viewer]") {
    const auto map = open_square();
    auto controller = viewer::Controller::at_start(map);
    const viewer::Actions forward{.forward = 1.0F};

    hold(controller, map, forward, 0.05F);
    const auto halfway = velocity_over_tick(controller, map, forward);
    hold(controller, map, forward, 0.5F);
    const auto walking = velocity_over_tick(controller, map, forward);

    CAPTURE(halfway, walking);
    CHECK(std::abs(halfway.x - 2.0F) < 0.2F);
    CHECK(wulfhilde::approx_equal(walking, Vector3{.x = 4.0F}, 1e-3F));
    CHECK(controller.grounded());
}

TEST_CASE("running reaches 8 m/s, and walking diagonally is no faster than walking", "[viewer]") {
    const auto map = open_square();
    auto running = viewer::Controller::at_start(map);
    auto diagonal = viewer::Controller::at_start(map);
    const viewer::Actions run{.forward = 1.0F, .run = true};
    const viewer::Actions forward_left{.forward = 1.0F, .strafe = 1.0F};

    hold(running, map, run, 0.5F);
    hold(diagonal, map, forward_left, 0.5F);

    CHECK(
        wulfhilde::approx_equal(velocity_over_tick(running, map, run), Vector3{.x = 8.0F}, 1e-3F));
    const auto half = 4.0F / std::numbers::sqrt2_v<float>;
    CHECK(wulfhilde::approx_equal(velocity_over_tick(diagonal, map, forward_left),
                                  Vector3{.x = half, .y = half}, 1e-3F));
}

TEST_CASE("the body walks where its eye turns, and across its gravity however far the eye looks "
          "up",
          "[viewer]") {
    const auto map = open_square();
    auto controller = viewer::Controller::at_start(map);
    const viewer::Actions forward{.forward = 1.0F};

    controller.look(Angle::degrees(90.0F), Angle::degrees(60.0F));
    hold(controller, map, forward, 0.5F);

    CHECK(wulfhilde::approx_equal(velocity_over_tick(controller, map, forward), Vector3{.y = 4.0F},
                                  1e-3F));
    CHECK(controller.grounded());
}

TEST_CASE("a body falls at 9.81 m/s^2, and steers in the air at 8 m/s^2", "[viewer]") {
    const auto map = square_map(100.0F, {.z = 50.0F});
    auto controller = viewer::Controller::at_start(map);
    REQUIRE(!controller.grounded());
    const auto start = center_of(controller);
    const viewer::Actions forward{.forward = 1.0F};

    hold(controller, map, forward, 0.25F);
    const auto velocity = velocity_over_tick(controller, map, forward);
    const auto fallen = start.z - center_of(controller).z;

    CAPTURE(velocity, fallen);
    // A quarter of a second, and a tick more.
    const auto seconds = 0.25F + viewer::Controller::tick_seconds;
    CHECK(std::abs(fallen - (0.5F * 9.81F * seconds * seconds)) < 0.02F);
    CHECK(std::abs(velocity.z + (9.81F * seconds)) < 0.05F);
    CHECK(std::abs(velocity.x - (8.0F * seconds)) < 0.05F);
    CHECK(!controller.grounded());
}

TEST_CASE("a body's speed is capped at 50 m/s", "[viewer]") {
    const auto map = square_map(1000.0F, {.z = 900.0F});
    auto controller = viewer::Controller::at_start(map);

    constexpr viewer::Actions falling{};
    hold(controller, map, falling, 6.0F);

    // Hundreds of metres up, a tick's move is only good to a few hundredths of a metre a second.
    CHECK(wulfhilde::approx_equal(velocity_over_tick(controller, map, {}), Vector3{.z = -50.0F},
                                  0.05F));
}

TEST_CASE("a jump from the ground reaches 1 m and lands again", "[viewer]") {
    const auto map = open_square();
    auto controller = viewer::Controller::at_start(map);
    const auto standing = center_of(controller).z;

    controller.tick(map, {.jump = true});
    auto highest = center_of(controller).z;
    for (std::size_t tick = 0; tick < 240 && !controller.grounded(); ++tick) {
        controller.tick(map, {});
        highest = std::max(highest, center_of(controller).z);
    }

    CAPTURE(highest - standing);
    CHECK(std::abs(highest - standing - 1.0F) < 0.02F);
    CHECK(controller.grounded());
    CHECK(std::abs(center_of(controller).z - standing) < 1e-3F);
}

TEST_CASE("a body jumps from the ground only, and again on landing while jump is held",
          "[viewer]") {
    const auto map = square_map(10.0F, {.z = 0.5F});
    auto controller = viewer::Controller::at_start(map);
    REQUIRE(!controller.grounded());
    const auto start = center_of(controller).z;

    // Falling half a metre takes about 0.32 s, and a jump's rise and fall 0.9 s.
    hold(controller, map, {.jump = true}, 0.3F);
    CHECK(center_of(controller).z < start);
    hold(controller, map, {.jump = true}, 0.4F);
    CHECK(center_of(controller).z > 1.2F);
}

TEST_CASE("the eye looks up and down no further than 89 degrees", "[viewer]") {
    const auto map = open_square();
    auto controller = viewer::Controller::at_start(map);

    controller.look(Angle::degrees(30.0F), Angle::degrees(120.0F));
    check_facing(controller.eye(map).facing, Angle::degrees(30.0F), Angle::degrees(89.0F), {});

    controller.look(Angle::degrees(-60.0F), Angle::degrees(-200.0F));
    check_facing(controller.eye(map).facing, Angle::degrees(-30.0F), Angle::degrees(-89.0F), {});
}

TEST_CASE("a camera is a viewpoint without a body, which looks about but doesn't move",
          "[viewer]") {
    const auto map = load_test_map("sectors");
    const wulfhilde::Location location{
        .sector = map.start().location.sector,
        .point = {.x = 3.0F, .y = 1.0F, .z = 2.5F},
    };

    auto controller = viewer::Controller::at_camera(location, Angle::degrees(45.0F),
                                                    Angle::degrees(-10.0F), Angle::degrees(5.0F));
    hold(controller, map, {.forward = 1.0F, .jump = true}, 0.5F);

    CHECK(!controller.body().has_value());
    CHECK(!controller.grounded());
    CHECK(controller.eye(map).location == location);
    check_facing(controller.eye(map).facing, Angle::degrees(45.0F), Angle::degrees(-10.0F),
                 Angle::degrees(5.0F));

    controller.look(Angle::degrees(10.0F), Angle::degrees(20.0F));
    CHECK(controller.eye(map).location == location);
    check_facing(controller.eye(map).facing, Angle::degrees(55.0F), Angle::degrees(10.0F),
                 Angle::degrees(5.0F));
}

TEST_CASE("returning to the start places a body there again, still and facing as the start faces",
          "[viewer]") {
    const auto map = open_square();
    auto walked = viewer::Controller::at_start(map);
    walked.look(Angle::degrees(70.0F), Angle::degrees(20.0F));
    hold(walked, map, {.forward = 1.0F, .run = true, .jump = true}, 0.6F);
    auto viewing = viewer::Controller::at_camera(
        {.sector = map.start().location.sector, .point = {.x = 5.0F, .z = 3.0F}}, {}, {}, {});

    walked.return_to_start(map);
    viewing.return_to_start(map);

    for (const auto* controller : {&walked, &viewing}) {
        CHECK(wulfhilde::approx_equal(center_of(*controller), Vector3{.z = 0.9F}, 1e-4F));
        CHECK(controller->grounded());
        check_facing(controller->eye(map).facing, {}, {}, {});
    }
    // Still: standing, it doesn't drift.
    CHECK(wulfhilde::approx_equal(velocity_over_tick(walked, map, {}), Vector3{}, 1e-4F));
}

TEST_CASE("a start position that the body doesn't fit gives a viewpoint at the eye, short of the "
          "ceiling, and a warning naming the start and the body's size",
          "[viewer]") {
    const auto map = square_map(1.2F, {.x = 1.0F, .y = -2.0F});

    auto controller = viewer::Controller::at_start(map);
    hold(controller, map, {.forward = 1.0F}, 0.5F);

    CHECK(!controller.body().has_value());
    const auto eye = controller.eye(map);
    CHECK(wulfhilde::approx_equal(eye.location.point, Vector3{.x = 1.0F, .y = -2.0F, .z = 1.1F},
                                  1e-4F));
    CHECK(viewer::unfit_start_warning(map) ==
          "the body, 0.3 m in radius and 1.8 m tall, doesn't fit at the start position, "
          "(1, -2, 0) in sector square, so the viewer looks from there without a body");
}

TEST_CASE("a copy of a controller goes its own way", "[viewer]") {
    const auto map = open_square();
    const auto original = viewer::Controller::at_start(map);
    auto copy = original;

    hold(copy, map, {.forward = 1.0F}, 0.5F);

    CHECK(wulfhilde::approx_equal(center_of(original), Vector3{.z = 0.9F}, 1e-4F));
    CHECK(center_of(copy).x > 1.0F);
}

TEST_CASE("a camera looking straight up or down keeps its pitch until the eye tilts", "[viewer]") {
    const auto map = load_test_map("sectors");
    auto controller = viewer::Controller::at_camera(
        {.sector = map.start().location.sector, .point = {.x = 2.0F, .y = 2.0F, .z = 2.0F}},
        Angle::degrees(10.0F), Angle::degrees(90.0F), Angle{});

    controller.look(Angle::degrees(20.0F), Angle{});
    check_facing(controller.eye(map).facing, Angle::degrees(30.0F), Angle::degrees(90.0F), {});

    controller.look(Angle{}, Angle::degrees(-5.0F));
    check_facing(controller.eye(map).facing, Angle::degrees(30.0F), Angle::degrees(85.0F), {});
}
