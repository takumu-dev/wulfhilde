// Scripted walks through the test maps: each walks the player's body through the controller and
// collision, and checks where it went.
#include "math_printing.hpp" // IWYU pragma: keep
#include "scripted_walk.hpp"
#include <wulfhilde/core/angle.hpp>
#include <wulfhilde/core/vector3.hpp>

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

TEST_CASE("sectors: walking across the hall, the body stops at its far wall", "[viewer]") {
    const auto map = load_test_map("sectors");

    const auto walked = walk(map, {
                                      {.actions = {.forward = 1.0F}, .timeout = 4.0F},
                                  });

    check_walk(walked, {.sectors = {"hall"}});
    // The far wall stands at x = 12 m, and the body is 0.3 m in radius.
    const auto end = walked.end.centre.point;
    CAPTURE(end);
    CHECK(std::abs(end.x - 11.7F) < 1e-3F);
}

TEST_CASE("sectors: from the hall, the body walks through the doorway into the gallery and on into "
          "the bay",
          "[viewer]") {
    const auto map = load_test_map("sectors");

    const auto walked = walk(map, {
                                      {
                                          .actions = {.forward = 1.0F},
                                          .turn = wulfhilde::Angle::degrees(90.0F),
                                          .until = "gallery",
                                          .timeout = 4.0F,
                                      },
                                      {.actions = {.forward = 1.0F}, .timeout = 0.5F},
                                      {
                                          .actions = {.forward = 1.0F},
                                          .turn = wulfhilde::Angle::degrees(-90.0F),
                                          .until = "bay",
                                          .timeout = 2.0F,
                                      },
                                  });

    check_walk(walked, {.sectors = {"hall", "gallery", "bay"}});
}

namespace {

// How far the player turns from the start's facing, along +X, to face a point from the start at
// (2, 2).
wulfhilde::Angle turn_toward(float x, float y) {
    return wulfhilde::Angle::radians(std::atan2(y - 2.0F, x - 2.0F));
}

} // namespace

TEST_CASE("sectors: the body walks up the stair onto the ledge without stopping", "[viewer]") {
    const auto map = load_test_map("sectors");

    // To the middle of the stair's foot, then up it. Each step is 0.3 m high and 0.5 m deep.
    const auto toward_stair = turn_toward(8.5F, 4.0F);
    const auto walked = walk(map, {
                                      {
                                          .actions = {.forward = 1.0F},
                                          .turn = toward_stair,
                                          .until = "stair-1",
                                          .timeout = 3.0F,
                                      },
                                      {
                                          .actions = {.forward = 1.0F},
                                          .turn = wulfhilde::Angle::degrees(90.0F) - toward_stair,
                                          .until = "ledge",
                                          .timeout = 1.0F,
                                      },
                                  });

    check_walk(walked, {.sectors = {"hall", "stair-1", "stair-2", "ledge"}});
    // Standing on the ledge, 0.9 m up.
    const auto end = walked.end.centre.point;
    CAPTURE(end);
    CHECK(std::abs(end.z - 1.8F) < 1e-3F);
}

TEST_CASE("sectors: the body walks through the doorway under its lower ceiling into the nook",
          "[viewer]") {
    const auto map = load_test_map("sectors");

    // The doorway is 2 m high, 0.2 m more than the body.
    const auto toward_doorway = turn_toward(5.75F, 4.0F);
    const auto walked = walk(map, {
                                      {
                                          .actions = {.forward = 1.0F},
                                          .turn = toward_doorway,
                                          .until = "doorway",
                                          .timeout = 3.0F,
                                      },
                                      {
                                          .actions = {.forward = 1.0F},
                                          .turn = wulfhilde::Angle::degrees(90.0F) - toward_doorway,
                                          .until = "nook",
                                          .timeout = 1.0F,
                                      },
                                  });

    check_walk(walked, {.sectors = {"hall", "doorway", "nook"}});
}

TEST_CASE("sectors: the step too high stops the body before it", "[viewer]") {
    const auto map = load_test_map("sectors");

    // The step, 0.6 m high, is in the hall's west wall, at x = 0, straight behind the start.
    const auto walked = walk(map, {
                                      {
                                          .actions = {.forward = 1.0F},
                                          .turn = wulfhilde::Angle::degrees(180.0F),
                                          .timeout = 2.0F,
                                      },
                                  });

    check_walk(walked, {.sectors = {"hall"}});
    const auto end = walked.end.centre.point;
    CAPTURE(end);
    CHECK(end.x < 0.5F);
    CHECK(std::abs(end.z - 0.9F) < 1e-3F);
}

TEST_CASE("sectors: the passage too low stops the body before it", "[viewer]") {
    const auto map = load_test_map("sectors");

    // The passage, 1.6 m high, opens from the hall's south wall, at y = 0, to the start's right.
    const auto walked = walk(map, {
                                      {
                                          .actions = {.forward = 1.0F},
                                          .turn = wulfhilde::Angle::degrees(-90.0F),
                                          .timeout = 2.0F,
                                      },
                                  });

    check_walk(walked, {.sectors = {"hall"}});
    const auto end = walked.end.centre.point;
    CAPTURE(end);
    CHECK(end.y < 0.5F);
    CHECK(std::abs(end.z - 0.9F) < 1e-3F);
}
