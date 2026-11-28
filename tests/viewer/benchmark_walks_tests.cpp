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
#include <wulfhilde/world/map.hpp>

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <format>
#include <limits>
#include <string>
#include <vector>

namespace {

using wulfhilde::Angle;
using wulfhilde::Vector2;

// How far an eye is from a point on the plan, across X and Y.
float distance_across(const viewer::Eye& eye, Vector2 point) {
    return std::hypot(eye.location.point.x - point.x, eye.location.point.y - point.y);
}

// How near a walk's frames came to the mapping camera's spot.
float nearest_mapping(const std::vector<viewer::WalkedFrame>& frames) {
    float nearest = std::numeric_limits<float>::infinity();
    for (const auto& frame : frames) {
        nearest = std::min(nearest, distance_across(frame.eye, {.x = 11.0F, .y = 1.0F}));
    }
    return nearest;
}

// The id of each sector a walk's frames were seen from, in order, with none twice in a row.
std::vector<std::string> sectors_seen_from(const wulfhilde::Map& map,
                                           const std::vector<viewer::WalkedFrame>& frames) {
    std::vector<std::string> sectors;
    for (const auto& frame : frames) {
        const auto& id = map.sector(frame.eye.location.sector).id;
        if (sectors.empty() || sectors.back() != id) {
            sectors.push_back(id);
        }
    }
    return sectors;
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

    // The tour lasts 19.5 s.
    CHECK(frames.size() == 1170);
    CHECK(viewer::frame_count(*walk) == 1170);
}

TEST_CASE("the sectors tour walks both arms of the hall, past the mapping camera's spot, through "
          "the gallery into the bay, and back to the start, facing as it faced",
          "[viewer]") {
    const auto map = load_test_map("sectors");
    const auto walk = viewer::find_benchmark_walk("sectors", "tour");
    REQUIRE(walk.has_value());

    const auto frames = viewer::walk_benchmark(map, *walk);
    REQUIRE(!frames.empty());

    CHECK(nearest_mapping(frames) < 1.0F);
    CHECK(sectors_seen_from(map, frames) ==
          std::vector<std::string>{"hall", "gallery", "bay", "gallery", "hall"});

    const auto& end = frames.back().eye;
    CAPTURE(end.location.point);
    CHECK(distance_across(end, {.x = 2.0F, .y = 2.0F}) < 1.0F);
    CHECK(wulfhilde::angle_between(end.facing.forward(), {.x = 1.0F}) < Angle::degrees(1.0F));
}

TEST_CASE("the outdoors tour crosses the bridge to the ledge and back, then walks into the field",
          "[viewer]") {
    const auto map = load_test_map("outdoors");
    const auto walk = viewer::find_benchmark_walk("outdoors", "tour");
    REQUIRE(walk.has_value());

    const auto frames = viewer::walk_benchmark(map, *walk);

    // The tour lasts 11.25 s.
    CHECK(frames.size() == 675);
    CHECK(sectors_seen_from(map, frames) ==
          std::vector<std::string>{"yard", "bridge", "ledge", "bridge", "yard", "field"});
}

TEST_CASE(
    "the overlap tour walks into the hall and back, across the bridge and back, then under it "
    "through the passage",
    "[viewer]") {
    const auto map = load_test_map("overlap");
    const auto walk = viewer::find_benchmark_walk("overlap", "tour");
    REQUIRE(walk.has_value());

    const auto frames = viewer::walk_benchmark(map, *walk);

    // The tour lasts 21.4 s.
    CHECK(frames.size() == 1284);
    CHECK(sectors_seen_from(map, frames) ==
          std::vector<std::string>{"yard", "hall", "yard", "bridge", "bank", "bridge", "yard",
                                   "chasm-west", "passage", "chasm-east"});
}

TEST_CASE("the slopes tour walks up the ramp onto the landing and back down, then into the sector "
          "whose floor and ceiling slope different ways",
          "[viewer]") {
    const auto map = load_test_map("slopes");
    const auto walk = viewer::find_benchmark_walk("slopes", "tour");
    REQUIRE(walk.has_value());

    const auto frames = viewer::walk_benchmark(map, *walk);

    // The tour lasts 10.5 s.
    CHECK(frames.size() == 630);
    CHECK(sectors_seen_from(map, frames) ==
          std::vector<std::string>{"hall", "ramp", "landing", "ramp", "hall", "opposed"});
}
