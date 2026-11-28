// Scripted walks through the test maps: each walks the player's body through the controller and
// collision, and checks where it went.
#include "controller.hpp"
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

TEST_CASE("sectors: walking at the pillar's south-west corner, the body slides round it and on "
          "past it",
          "[viewer]") {
    const auto map = load_test_map("sectors");

    // The pillar stands from (3, 6) to (3.6, 6.6), and the body's line passes 0.08 m west of its
    // corner, well within the body's radius of 0.3 m.
    const auto walked = walk(map, {
                                      {
                                          .actions = {.forward = 1.0F},
                                          .turn = turn_toward(2.92F, 6.0F),
                                          .timeout = 2.0F,
                                      },
                                  });

    check_walk(walked, {.sectors = {"hall"}});
    // North of the pillar and east of its west wall, so round it, but turned west of the line the
    // body set out on, which runs through x = 3.6 m at y = 9 m.
    const auto end = walked.end.centre.point;
    CAPTURE(end);
    CHECK(end.y > 9.0F);
    CHECK(end.x > 3.0F);
    CHECK(end.x < 3.4F);
}

TEST_CASE("sectors: the body steps onto the dais through its inner loop's opening", "[viewer]") {
    const auto map = load_test_map("sectors");

    // The dais stands 0.3 m up, from (0.4, 7) to (1.4, 9).
    const auto walked = walk(map, {
                                      {
                                          .actions = {.forward = 1.0F},
                                          .turn = turn_toward(0.9F, 8.0F),
                                          .until = "dais",
                                          .timeout = 3.0F,
                                      },
                                      {.actions = {.forward = 1.0F}, .timeout = 0.1F},
                                  });

    check_walk(walked, {.sectors = {"hall", "dais"}});
    const auto end = walked.end.centre.point;
    CAPTURE(end);
    CHECK(std::abs(end.z - 1.2F) < 1e-3F);
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

TEST_CASE("outdoors: walking south from the start, the body crosses the bridge over the abyss to "
          "the ledge beyond",
          "[viewer]") {
    const auto map = load_test_map("outdoors");

    const auto walked =
        walk(map, {
                      {.actions = {.forward = 1.0F}, .until = "ledge", .timeout = 6.0F},
                  });

    check_walk(walked, {.sectors = {"yard", "bridge", "ledge"}});
}

TEST_CASE("outdoors: stepping off the bridge, the body falls into the abyss and stands on its sky "
          "floor",
          "[viewer]") {
    const auto map = load_test_map("outdoors");

    const auto walked = walk(map, {
                                      {
                                          .actions = {.forward = 1.0F},
                                          .until = "bridge",
                                          .timeout = 4.0F,
                                      },
                                      {.actions = {.forward = 1.0F}, .timeout = 1.0F},
                                      {
                                          .actions = {.forward = 1.0F},
                                          .turn = wulfhilde::Angle::degrees(90.0F),
                                          .timeout = 3.0F,
                                      },
                                  });

    check_walk(walked, {.sectors = {"yard", "bridge", "abyss-east"}});
    // The abyss's sky floor lies at -8 m, and the body's centre stands 0.9 m above what it stands
    // on.
    const auto end = walked.end.centre.point;
    CAPTURE(end);
    CHECK(std::abs(end.z - -7.1F) < 1e-3F);
}

TEST_CASE("lights: from the hall, the body walks east through the doorway into the annex",
          "[viewer]") {
    const auto map = load_test_map("lights");

    const auto walked =
        walk(map, {
                      {.actions = {.forward = 1.0F}, .until = "annex", .timeout = 4.0F},
                  });

    check_walk(walked, {.sectors = {"hall", "annex"}});
}

TEST_CASE("overlap: walking south from the start, the body crosses the bridge to the bank, staying "
          "on it",
          "[viewer]") {
    const auto map = load_test_map("overlap");

    const auto walked = walk(map, {
                                      {
                                          .actions = {.forward = 1.0F},
                                          .turn = wulfhilde::Angle::degrees(180.0F),
                                          .until = "bank",
                                          .timeout = 4.0F,
                                      },
                                  });

    check_walk(walked, {.sectors = {"yard", "bridge", "bank"}});
    // The bank's floor is at the bridge's height, and the body's centre stands 0.9 m above it.
    const auto end = walked.end.centre.point;
    CAPTURE(end);
    CHECK(std::abs(end.z - 0.9F) < 1e-3F);
}

TEST_CASE("overlap: dropping into the chasm beside the bridge, the body walks under it through the "
          "passage, staying in the passage, to the chasm beyond",
          "[viewer]") {
    const auto map = load_test_map("overlap");

    const auto walked = walk(map, {
                                      {
                                          .actions = {.forward = 1.0F},
                                          .turn = wulfhilde::Angle::degrees(90.0F),
                                          .timeout = 1.0F,
                                      },
                                      {
                                          .actions = {.forward = 1.0F},
                                          .turn = wulfhilde::Angle::degrees(90.0F),
                                          .until = "chasm-west",
                                          .timeout = 2.0F,
                                      },
                                      {.actions = {.forward = 1.0F}, .timeout = 0.7F},
                                      {.timeout = 1.0F},
                                      {
                                          .actions = {.forward = 1.0F},
                                          .turn = wulfhilde::Angle::degrees(90.0F),
                                          .until = "chasm-east",
                                          .timeout = 3.0F,
                                      },
                                  });

    check_walk(walked, {.sectors = {"yard", "chasm-west", "passage", "chasm-east"}});
    // The chasm's floor lies 3 m down, as the passage's does.
    const auto end = walked.end.centre.point;
    CAPTURE(end);
    CHECK(std::abs(end.z - -2.1F) < 1e-3F);
}

TEST_CASE("overlap: from the yard, the body walks through the hut's door into the hall, which "
          "shares the yard's space, and back out to the yard",
          "[viewer]") {
    const auto map = load_test_map("overlap");

    const auto walked =
        walk(map, {
                      {.actions = {.forward = 1.0F}, .until = "hall", .timeout = 2.0F},
                      {.actions = {.forward = 1.0F}, .timeout = 1.0F},
                      {
                          .actions = {.forward = 1.0F},
                          .turn = wulfhilde::Angle::degrees(180.0F),
                          .until = "yard",
                          .timeout = 2.0F,
                      },
                  });

    check_walk(walked, {.sectors = {"yard", "hall", "yard"}});
}

namespace {

// How far the player turns from the start's facing, along +X, to face a point of the slopes map
// from the start at (6, 4).
wulfhilde::Angle turn_in_slopes_toward(float x, float y) {
    return wulfhilde::Angle::radians(std::atan2(y - 4.0F, x - 6.0F));
}

} // namespace

TEST_CASE("slopes: the body walks up the ramp onto the landing, and back down into the hall",
          "[viewer]") {
    const auto map = load_test_map("slopes");

    // To the middle of the ramp's foot, then straight up it, north-east, and back.
    const auto toward_ramp = turn_in_slopes_toward(11.0F, 7.0F);
    const auto walked = walk(map, {
                                      {
                                          .actions = {.forward = 1.0F},
                                          .turn = toward_ramp,
                                          .until = "ramp",
                                          .timeout = 3.0F,
                                      },
                                      {
                                          .actions = {.forward = 1.0F},
                                          .turn = wulfhilde::Angle::degrees(45.0F) - toward_ramp,
                                          .until = "landing",
                                          .timeout = 2.0F,
                                      },
                                      {.actions = {.forward = 1.0F}, .timeout = 0.25F},
                                      {
                                          .actions = {.forward = 1.0F},
                                          .turn = wulfhilde::Angle::degrees(180.0F),
                                          .until = "hall",
                                          .timeout = 3.0F,
                                      },
                                      {.actions = {.forward = 1.0F}, .timeout = 0.25F},
                                  });

    check_walk(walked, {.sectors = {"hall", "ramp", "landing", "ramp", "hall"}});
    // Standing on the hall's floor again.
    const auto end = walked.end.centre.point;
    CAPTURE(end);
    CHECK(std::abs(end.z - 0.9F) < 1e-3F);
}

TEST_CASE("slopes: the body walks up the gentle slope to its far wall", "[viewer]") {
    const auto map = load_test_map("slopes");

    // To the middle of the slope's foot, then up it, south, to its far wall at y = -1.5.
    const auto toward_slope = turn_in_slopes_toward(3.5F, 0.0F);
    const auto walked = walk(map, {
                                      {
                                          .actions = {.forward = 1.0F},
                                          .turn = toward_slope,
                                          .until = "gentle",
                                          .timeout = 3.0F,
                                      },
                                      {
                                          .actions = {.forward = 1.0F},
                                          .turn = wulfhilde::Angle::degrees(-90.0F) - toward_slope,
                                          .timeout = 1.5F,
                                      },
                                  });

    check_walk(walked, {.sectors = {"hall", "gentle"}});
    // Against the far wall, a radius from it, standing on the slope, which rises 0.577 m a metre:
    // the lower cap's centre is a radius from the slope, square to it, 0.3464 m above it.
    const auto end = walked.end.centre.point;
    CAPTURE(end);
    CHECK(std::abs(end.y - -1.2F) < 1e-3F);
    CHECK(std::abs(end.z - ((0.57735027F * 1.2F) + 0.34641016F + 0.6F)) < 1e-3F);
}

TEST_CASE("slopes: walking at the steep slope, the body can't climb it, and ends below it",
          "[viewer]") {
    const auto map = load_test_map("slopes");

    // Straight at the steep slope, south of a point of the start's row in front of it.
    const auto toward_slope = turn_in_slopes_toward(8.5F, 0.0F);
    const auto walked = walk(map, {
                                      {
                                          .actions = {.forward = 1.0F},
                                          .turn = toward_slope,
                                          .timeout = 3.0F,
                                      },
                                      {.timeout = 1.0F},
                                  });

    check_walk(walked, {.sectors = {"hall"}});
    // On the hall's floor, short of the slope's foot at y = 0.
    const auto end = walked.end.centre.point;
    CAPTURE(end);
    CHECK(end.y > 0.0F);
    CHECK(end.y < 0.5F);
    CHECK(std::abs(end.z - 0.9F) < 1e-3F);
}

TEST_CASE("portals: walking round the loop of three corners, the body comes back into the start "
          "room through the turned portal, turned with it",
          "[viewer]") {
    const auto map = load_test_map("portals");
    const viewer::Actions forward{.forward = 1.0F};
    const auto right = wulfhilde::Angle::degrees(-90.0F);

    // North up the leg to its end, east along the top leg to its end, then south down the east
    // leg, whose end leads west into the start room.
    const auto walked = walk(map, {
                                      {.actions = forward, .until = "north-leg", .timeout = 2.0F},
                                      {.actions = forward, .timeout = 2.75F},
                                      {
                                          .actions = forward,
                                          .turn = right,
                                          .until = "top-leg",
                                          .timeout = 1.0F,
                                      },
                                      {.actions = forward, .timeout = 2.5F},
                                      {
                                          .actions = forward,
                                          .turn = right,
                                          .until = "east-leg",
                                          .timeout = 1.0F,
                                      },
                                      {.actions = forward, .until = "start", .timeout = 4.0F},
                                      {.actions = forward, .timeout = 0.25F},
                                  });

    check_walk(walked, {
                           .sectors = {"start", "north-leg", "top-leg", "east-leg", "start"},
                           .turned = right,
                       });
    // Walking west through the start room from its east doorway.
    const auto end = walked.end.centre.point;
    CAPTURE(end);
    CHECK(end.x < 6.0F);
    CHECK(end.y > 2.0F);
    CHECK(end.y < 4.0F);
}

TEST_CASE("portals: from the start room, the body walks west through the translated portal into "
          "the annex, 4 km away and 100 m up",
          "[viewer]") {
    const auto map = load_test_map("portals");

    const auto walked = walk(map, {
                                      {
                                          .actions = {.forward = 1.0F},
                                          .turn = wulfhilde::Angle::degrees(90.0F),
                                          .until = "annex",
                                          .timeout = 3.0F,
                                      },
                                      {.actions = {.forward = 1.0F}, .timeout = 0.25F},
                                  });

    check_walk(walked, {.sectors = {"start", "annex"}, .turned = wulfhilde::Angle{}});
    const auto end = walked.end.centre.point;
    CAPTURE(end);
    CHECK(std::abs(end.z - 100.9F) < 1e-3F);
    CHECK(end.x < 2987.0F);
}
