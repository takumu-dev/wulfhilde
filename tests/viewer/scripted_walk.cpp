#include "scripted_walk.hpp"

#include "collision_printing.hpp" // IWYU pragma: keep
#include "controller.hpp"
#include <wulfhilde/assets/content_root.hpp>
#include <wulfhilde/assets/load_map.hpp>
#include <wulfhilde/collision/pose.hpp>
#include <wulfhilde/core/angle.hpp>
#include <wulfhilde/core/narrow.hpp>
#include <wulfhilde/core/rotation.hpp>
#include <wulfhilde/core/vector3.hpp>
#include <wulfhilde/world/map.hpp>

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstddef>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

std::size_t ticks_in(float seconds) {
    return wulfhilde::narrow<std::size_t>(std::lround(seconds / viewer::Controller::tick_seconds));
}

wulfhilde::Map load_test_map(std::string_view name) {
    const auto root = wulfhilde::ContentRoot::open(WULFHILDE_TEST_CONTENT);
    REQUIRE(root.has_value());
    auto map = wulfhilde::load_map(*root, name);
    if (!map) {
        FAIL(std::format("{}", map.error()));
    }
    return *std::move(map);
}

Walked walk(const wulfhilde::Map& map, const std::vector<WalkStep>& steps) {
    auto controller = viewer::Controller::at_start(map);
    if (!controller.body()) {
        FAIL(viewer::unfit_start_warning(map));
    }
    const auto body = [&controller] { return controller.body().value(); };
    const auto sector_id = [&map, &body] { return map.sector(body().centre.sector).id; };

    std::vector<std::string> sectors{sector_id()};
    for (const auto& step : steps) {
        controller.look(step.turn, {});
        const auto ticks = ticks_in(step.timeout);
        for (std::size_t tick = 0; tick < ticks && sector_id() != step.until; ++tick) {
            controller.tick(map, step.actions);
            if (sector_id() != sectors.back()) {
                sectors.push_back(sector_id());
            }
        }
    }
    return {.sectors = std::move(sectors), .end = body()};
}

namespace {

// Checks that a body ended turned left about its up by an angle, within 1 degree, if a walk
// checks that.
void check_turned(const wulfhilde::Pose& end, const std::optional<wulfhilde::Angle>& turned) {
    if (turned) {
        CHECK(wulfhilde::approx_equal(end.tilt,
                                      wulfhilde::Rotation::from_yaw_pitch_roll(*turned, {}, {}),
                                      wulfhilde::Angle::degrees(1.0F)));
    }
}

} // namespace

void check_walk(const Walked& walked, const WalkExpectation& expected) {
    CAPTURE(walked.end);
    CHECK(walked.sectors == expected.sectors);
    if (!expected.sectors.empty()) {
        CHECK(walked.sectors.back() == expected.sectors.back());
    }
    const auto& end = walked.end;
    CHECK(wulfhilde::angle_between(end.tilt.up(), end.gravity_up) <=
          wulfhilde::Angle::degrees(1.0F));
    if (expected.gravity_up) {
        CHECK(wulfhilde::angle_between(end.gravity_up, *expected.gravity_up) <=
              wulfhilde::Angle::degrees(1.0F));
    }
    check_turned(end, expected.turned);
}
