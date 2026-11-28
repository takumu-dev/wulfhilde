// Scripted walks: the player's body walked through a test map by the controller, in goal-based
// steps.
#pragma once

#include "controller.hpp"
#include <wulfhilde/collision/pose.hpp>
#include <wulfhilde/core/angle.hpp>
#include <wulfhilde/core/vector3.hpp>
#include <wulfhilde/world/map.hpp>

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// One step of a walk: actions held, after a turn, until the body is in a sector or a time passes.
struct WalkStep {
    // What the player asks for, each tick of the step.
    viewer::Actions actions{};

    // How far the player turns left as the step begins.
    wulfhilde::Angle turn{};

    // The id of the sector the step walks to, which ends it as soon as the body's centre is in it,
    // or nothing to hold the actions for the whole timeout.
    std::optional<std::string> until{};

    // How long the step lasts at most, in seconds.
    float timeout = 0.0F;
};

// Where a walk went.
struct Walked {
    // The id of each sector the body's centre was in, in order, from the start position's, with
    // none twice in a row.
    std::vector<std::string> sectors;

    // Where the body ended.
    wulfhilde::Pose end;
};

// What a walk must come to.
struct WalkExpectation {
    // The id of each sector the body's centre was in, in order, from the start position's. The
    // last is the sector the walk ends in.
    std::vector<std::string> sectors;

    // The body's gravity up at the end, if the walk checks it.
    std::optional<wulfhilde::Vector3> gravity_up{};

    // How far the portals the body crossed turned it left about its up, if the walk checks it: its
    // tilt at the end, when it ends upright.
    std::optional<wulfhilde::Angle> turned{};
};

// Returns how many of the controller's ticks last a time, in seconds, to the nearest tick.
[[nodiscard]] std::size_t ticks_in(float seconds);

// Loads a test map, by its asset path in the test content, failing the test if it doesn't load.
[[nodiscard]] wulfhilde::Map load_test_map(std::string_view name);

// Walks a body from the map's start position, step by step, failing the test if the body doesn't
// fit there.
[[nodiscard]] Walked walk(const wulfhilde::Map& map, const std::vector<WalkStep>& steps);

// Checks that a walk crossed the sectors expected in order, ended in the last of them, and ended
// upright by its gravity, with the gravity up expected if there is one, and turned as expected if
// the walk checks that, within 1 degree.
void check_walk(const Walked& walked, const WalkExpectation& expected);
