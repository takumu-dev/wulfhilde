// The benchmark walks: each is found by its map and name, and the walks of the test maps go where
// their maps need them to.
#include "benchmark_walks.hpp"

#include "command_line.hpp"
#include "controller.hpp"
#include "math_printing.hpp" // IWYU pragma: keep
#include "scripted_walk.hpp"
#include <wulfhilde/core/angle.hpp>
#include <wulfhilde/core/vector2.hpp>
#include <wulfhilde/core/vector3.hpp>

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <format>
#include <limits>
#include <vector>

namespace {

using wulfhilde::Angle;
using wulfhilde::Vector2;

// How far an eye is from a point on the plan, across X and Y.
float distance_across(const viewer::Eye& eye, Vector2 point) {
    return std::hypot(eye.location.point.x - point.x, eye.location.point.y - point.y);
}

// How near a walk's frames came to the mapping camera's spot, and how far north they went.
struct Reach {
    float nearest_mapping = std::numeric_limits<float>::infinity();
    float furthest_north = -std::numeric_limits<float>::infinity();
};

Reach reach_of(const std::vector<viewer::WalkedFrame>& frames) {
    Reach reach;
    for (const auto& frame : frames) {
        reach.nearest_mapping =
            std::min(reach.nearest_mapping, distance_across(frame.eye, {.x = 11.0F, .y = 1.0F}));
        reach.furthest_north = std::max(reach.furthest_north, frame.eye.location.point.y);
    }
    return reach;
}

} // namespace

TEST_CASE("a benchmark walk is found by its map's asset path and its name", "[viewer]") {
    const auto walk = viewer::find_benchmark_walk("sectors", "tour");

    REQUIRE(walk.has_value());
    CHECK(walk->map == "sectors");
    CHECK(walk->name == "tour");
}

TEST_CASE("a benchmark walk the map lacks is a usage error naming the walks it has", "[viewer]") {
    const auto walk = viewer::find_benchmark_walk("sectors", "marathon");

    REQUIRE(!walk.has_value());
    CHECK(walk.error().code == viewer::UsageErrorCode::unknown_walk);
    CHECK(std::format("{}", walk.error()) ==
          "--benchmark names no benchmark walk of sectors: marathon, only tour");
}

TEST_CASE("a map with no benchmark walk is a usage error saying so", "[viewer]") {
    const auto walk = viewer::find_benchmark_walk("maps/e1m1", "tour");

    REQUIRE(!walk.has_value());
    CHECK(walk.error().code == viewer::UsageErrorCode::unknown_walk);
    CHECK(std::format("{}", walk.error()) == "maps/e1m1 has no benchmark walk to play");
}

TEST_CASE("a benchmark walk draws a frame every 4 ticks, at 60 a second", "[viewer]") {
    const auto map = load_test_map("sectors");
    const auto walk = viewer::find_benchmark_walk("sectors", "tour");
    REQUIRE(walk.has_value());

    const auto frames = viewer::walk_benchmark(map, *walk);

    // The tour lasts 13 s.
    CHECK(frames.size() == 780);
    CHECK(viewer::frame_count(*walk) == 780);
}

TEST_CASE("the sectors tour walks both arms of the hall, past the mapping camera's spot, and "
          "back to the start, facing as it faced",
          "[viewer]") {
    const auto map = load_test_map("sectors");
    const auto walk = viewer::find_benchmark_walk("sectors", "tour");
    REQUIRE(walk.has_value());

    const auto frames = viewer::walk_benchmark(map, *walk);
    REQUIRE(!frames.empty());

    const auto reach = reach_of(frames);
    CHECK(reach.nearest_mapping < 1.0F);
    // The arm along Y runs to y = 10 m.
    CHECK(reach.furthest_north > 8.5F);

    const auto& end = frames.back().eye;
    CAPTURE(end.location.point);
    CHECK(distance_across(end, {.x = 2.0F, .y = 2.0F}) < 1.0F);
    CHECK(wulfhilde::angle_between(end.facing.forward(), {.x = 1.0F}) < Angle::degrees(1.0F));
}
