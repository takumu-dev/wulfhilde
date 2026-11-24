// Scripted walks through the test maps: each walks the player's body through the controller and
// collision, and checks where it went.
#include "math_printing.hpp" // IWYU pragma: keep
#include "scripted_walk.hpp"
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
